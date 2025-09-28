#include "Shell.hpp"

#include <iostream>
#include <sstream>
#include <cctype>
#include <atomic>
#include <csignal>
#ifdef _WIN32
#  include <windows.h>
#endif

#include "Parser.hpp"
#include "ICommand.hpp"
#include "CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "../core/Environment.hpp"
#include "../core/Interrupt.hpp"

// Forward declare factory to register commands
namespace Builtins { void register_all(CommandRegistry& reg); }
// Expose registry for Help command
CommandRegistry* g_registry_for_help = nullptr;

// SIGINT (Ctrl+C) handling
static std::atomic<bool> s_interrupted{false};
static void on_sigint(int) { s_interrupted.store(true); Interrupt::set(); }

#ifdef _WIN32
static BOOL WINAPI on_console_ctrl(DWORD type) {
    if (type == CTRL_C_EVENT) {
        s_interrupted.store(true);
        Interrupt::set();
        return TRUE; // handled; do not terminate process
    }
    return FALSE;
}
#endif

Shell::Shell(std::istream& in, std::ostream& out, IVfs& vfs, Environment& env)
    : in_(in), out_(out), vfs_(vfs), env_(env) {
    // Default cwd
    cwd_ = std::filesystem::path("/");
    register_builtin_commands();
}

void Shell::register_builtin_commands() {
    Builtins::register_all(registry_);
    g_registry_for_help = &registry_;
}

// String helpers
static std::string s_ltrim(const std::string& s){ size_t i=0; while(i<s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; return s.substr(i); }
static std::string s_rtrim(const std::string& s){ size_t j=s.size(); while(j>0 && std::isspace(static_cast<unsigned char>(s[j-1]))) --j; return s.substr(0,j); }
std::string Shell::ltrim(const std::string& s){ return s_ltrim(s); }
std::string Shell::rtrim(const std::string& s){ return s_rtrim(s); }
std::string Shell::trim(const std::string& s){ return s_rtrim(s_ltrim(s)); }

std::string Shell::expand_vars(const std::string& input, const Environment& env) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ) {
        char c = input[i];
        if (c == '$') {
            if (i + 1 < input.size() && input[i+1] == '{') {
                size_t j = i + 2; // after ${
                while (j < input.size() && input[j] != '}') ++j;
                if (j < input.size()) {
                    std::string key = input.substr(i + 2, j - (i + 2));
                    out += env.get(key);
                    i = j + 1;
                    continue;
                }
                // no closing }, treat as literal
            } else {
                // $VAR form (including $? and numeric positional $1..$9)
                size_t j = i + 1;
                if (j < input.size() && input[j] == '?') {
                    out += env.get("?");
                    i = j + 1;
                    continue;
                }
                if (j < input.size() && (std::isalpha(static_cast<unsigned char>(input[j])) || input[j] == '_' || std::isdigit(static_cast<unsigned char>(input[j])))) {
                    ++j;
                    while (j < input.size() && (std::isalnum(static_cast<unsigned char>(input[j])) || input[j] == '_')) ++j;
                    std::string key = input.substr(i + 1, j - (i + 1));
                    out += env.get(key);
                    i = j;
                    continue;
                }
            }
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

bool Shell::has_exec_permission(const std::filesystem::path& host_path) const {
    try {
        auto execdb = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/execdb"));
        auto data = vfs_.readFile(execdb);
        std::istringstream is(data);
        std::string line;
        auto target = host_path.generic_string();
        while (std::getline(is, line)) {
            line = trim(line);
            if (line == target) return true;
        }
    } catch (const std::exception&) {
        // no db yet
    }
    return false;
}

int Shell::execute_line(const std::string& line) {
    return execute_line_with_env(line, env_);
}

int Shell::execute_line_with_env(const std::string& raw_line, Environment& active_env) {
    auto raw = trim(raw_line);
    if (raw.empty()) return 0;
    if (!raw.empty() && raw[0] == '#') return 0;
    if (active_env.get("?").empty()) active_env.set("?", "0");

    // Simple variable assignment KEY=VALUE (no spaces around '=')
    {
        auto pos = raw.find('=');
        if (pos != std::string::npos) {
            bool has_space = (raw.find(' ') != std::string::npos) || (raw.find('\t') != std::string::npos);
            if (!has_space) {
                std::string key = raw.substr(0, pos);
                std::string val = raw.substr(pos + 1);
                // Strip symmetrical quotes
                if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                    val = val.substr(1, val.size()-2);
                }
                val = expand_vars(val, active_env);
                active_env.set(key, val);
                return 0;
            }
        }
    }

    auto tokens = Parser::split(raw);
    if (tokens.empty()) return 0;

    // Variable expansion in tokens (MVP: expand everywhere)
    for (auto& t : tokens) {
        if (t == "|" || t == "<" || t == ">" || t == ">>") continue;
        t = expand_vars(t, active_env);
    }

    // Built-in: source <path>
    if (!tokens.empty() && tokens[0] == "source") {
        if (tokens.size() < 2) { out_ << "source: missing path" << std::endl; return 2; }
        try {
            auto abs = vfs_.resolveSecure(cwd_, tokens[1]);
            int rc = execute_script_file(abs, /*source_mode*/true, active_env);
            active_env.set("?", std::to_string(rc));
            return rc;
        } catch (const std::exception& e) {
            out_ << "source: " << e.what() << std::endl; return 1;
        }
    }

    // Direct script execution by path
    try {
        const std::string& cmd0 = tokens[0];
        bool looks_like_path = !cmd0.empty() && (cmd0[0] == '/' || cmd0[0] == '.' || cmd0.find('/') != std::string::npos);
        if (looks_like_path) {
            auto abs = vfs_.resolveSecure(cwd_, cmd0);
            auto st = vfs_.stat(abs);
            if (!st.is_dir) {
                if (!has_exec_permission(abs)) { out_ << "permission denied: " << cmd0 << std::endl; return 126; }
                std::vector<std::string> args;
                if (tokens.size() > 1) args.assign(tokens.begin()+1, tokens.end());
                int rc = execute_script_file(abs, /*source_mode*/false, active_env, args);
                active_env.set("?", std::to_string(rc));
                return rc;
            }
        }
    } catch (const std::exception&) {
        // fallthrough
    }

    // Split by pipeline '|'
    std::vector<std::vector<std::string>> segments(1);
    for (const auto& t : tokens) {
        if (t == "|") segments.emplace_back();
        else segments.back().push_back(t);
    }
    if (segments.empty() || segments[0].empty()) return 0;

    // Parse redirections: input '<' on first; output '>'/ '>>' on last
    auto parse_redir = [](const std::vector<std::string>& in,
                          bool allow_in, bool allow_out,
                          std::vector<std::string>& args,
                          std::string& in_file,
                          std::string& out_file,
                          bool& out_append) -> int {
        args.clear(); in_file.clear(); out_file.clear(); out_append = false;
        for (size_t i = 0; i < in.size(); ++i) {
            const std::string& tok = in[i];
            if (allow_in && (tok == "<" || (!tok.empty() && tok[0] == '<'))) {
                std::string rest = (tok == "<" ? std::string() : tok.substr(1));
                if (rest.empty()) {
                    if (i + 1 >= in.size()) return 2; // missing target
                    in_file = in[++i];
                } else in_file = rest;
                continue;
            }
            if (allow_out && (tok == ">" || tok == ">>" || (!tok.empty() && tok[0] == '>'))) {
                out_append = (tok == ">>") || (tok.rfind(">>", 0) == 0);
                std::string rest = (tok == ">" || tok == ">>" ? std::string() : tok.substr(out_append ? 2 : 1));
                if (rest.empty()) {
                    if (i + 1 >= in.size()) return 2; // missing target
                    out_file = in[++i];
                } else out_file = rest;
                continue;
            }
            args.push_back(tok);
        }
        return 0;
    };

    std::string first_in_file, last_out_file; bool last_out_append = false;
    std::vector<std::string> stage_args;
    // Validate and strip redirections
    for (size_t si = 0; si < segments.size(); ++si) {
        bool allow_in = (si == 0);
        bool allow_out = (si + 1 == segments.size());
        std::vector<std::string> args;
        std::string in_file, out_file; bool out_append = false;
        int rc = parse_redir(segments[si], allow_in, allow_out, args, in_file, out_file, out_append);
        if (rc != 0) { out_ << "syntax error: missing redirection target" << std::endl; return 2; }
        if (args.empty()) { out_ << "syntax error: empty command" << std::endl; return 2; }
        segments[si] = args; // store cleaned args
        if (allow_in) first_in_file = in_file;
        if (allow_out) { last_out_file = out_file; last_out_append = out_append; }
    }

    // Prepare input redirection if any
    std::string in_data;
    std::istringstream in_buf;
    std::istream* current_in = &in_;
    if (!first_in_file.empty()) {
        try {
            auto abs = vfs_.resolveSecure(cwd_, first_in_file);
            in_data = vfs_.readFile(abs);
            in_buf.str(in_data);
            current_in = &in_buf;
        } catch (const std::exception& e) {
            out_ << "redirect: " << e.what() << std::endl; return 1;
        }
    }

    std::string pipe_data;
    for (size_t si = 0; si < segments.size(); ++si) {
        auto& args = segments[si];
        auto* cmd = registry_.find(args[0]);
        if (!cmd) { out_ << args[0] << ": command not found" << std::endl; return 127; }

        bool last = (si + 1 == segments.size());
        std::ostringstream out_buf;
        std::ostream* out_stream = (!last || !last_out_file.empty()) ? static_cast<std::ostream*>(&out_buf) : &out_;

        CommandContext ctx(args, *current_in, *out_stream, vfs_, active_env, cwd_);
        int rc = cmd->execute(ctx);
        if (rc != 0) { active_env.set("?", std::to_string(rc)); return rc; }

        // Prepare input for next stage
        pipe_data = out_buf.str();
        if (!last) {
            in_buf.clear();
            in_buf.str(pipe_data);
            current_in = &in_buf;
        } else if (!last_out_file.empty()) {
            try {
                auto abs_out = vfs_.resolveSecure(cwd_, last_out_file);
                vfs_.writeFile(abs_out, pipe_data, last_out_append);
            } catch (const std::exception& e) {
                out_ << "redirect: " << e.what() << std::endl; return 1;
            }
        }
    }
    active_env.set("?", "0");
    return 0;
}

int Shell::run() {
    // Install Ctrl+C handler(s)
#ifdef _WIN32
    SetConsoleCtrlHandler(on_console_ctrl, TRUE);
    // Prevent CRT from terminating the process on SIGINT
    std::signal(SIGINT, SIG_IGN);
#else
    using HandlerT = void(*)(int);
    HandlerT prev_handler = std::signal(SIGINT, on_sigint);
#endif

    // First-run username setup (persisted in VFS at /etc/username)
    auto trim = [](std::string s){
        size_t i = 0; while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        size_t j = s.size(); while (j > i && std::isspace(static_cast<unsigned char>(s[j-1]))) --j;
        return s.substr(i, j - i);
    };

    std::string user = env_.get("USER");
    if (user.empty()) {
        try {
            auto host_user = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/username"));
            auto data = vfs_.readFile(host_user);
            data = trim(data);
            if (!data.empty()) {
                env_.set("USER", data);
                user = data;
            }
        } catch (const std::exception&) {
            // ignore; will prompt if not found
        }
    }
    bool first_run = false;
    // Determine if welcome should be shown (first application run)
    try {
        auto welcome_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/welcome_shown"));
        (void)vfs_.readFile(welcome_host);
    } catch (const std::exception&) {
        first_run = true;
    }

    if (first_run) {
        out_ << "Cortex v0.1 -- The Core Shell Environment" << std::endl;
        out_ << "(c) 2025 [Your Name]. Type 'help' for a list of commands." << std::endl;
    }

    if (user.empty()) {
        out_ << "No username found. Let's create one." << std::endl;
        std::string uname;
        while (true) {
            out_ << "Enter a username: ";
            if (!std::getline(in_, uname)) return 0;
            uname = trim(uname);
            if (!uname.empty()) break;
            out_ << "Username cannot be empty." << std::endl;
        }
        env_.set("USER", uname);
        try {
            auto etc_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc"));
            vfs_.mkdir(etc_host, true);
            auto file_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/username"));
            vfs_.writeFile(file_host, uname + "\n", false);
            if (first_run) {
                auto welcome_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/welcome_shown"));
                vfs_.writeFile(welcome_host, std::string("1\n"), false);
            }
        } catch (const std::exception& e) {
            out_ << "warning: failed to save username: " << e.what() << std::endl;
        }
    } else if (first_run) {
        // Mark welcome as shown even if username already existed
        try {
            auto etc_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc"));
            vfs_.mkdir(etc_host, true);
            auto welcome_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/welcome_shown"));
            vfs_.writeFile(welcome_host, std::string("1\n"), false);
        } catch (const std::exception&) {
            // ignore
        }
    }

    std::string line;
    while (true) {
        // reset interrupt flag at the top of loop for fresh command entry
        Interrupt::clear();
        out_ << prompt_user() << "@cortex:" << prompt_path_display() << "$ ";
        if (!std::getline(in_, line)) {
            if (s_interrupted.exchange(false)) {
                // Clear any partial input and continue without exiting
                in_.clear();
                // Drain remainder of the line if buffered
                auto* rb = in_.rdbuf();
                if (rb && rb->in_avail() > 0) {
                    std::string dummy;
                    std::getline(in_, dummy);
                }
                // Show ^C when no program is running (interrupting input)
                out_ << "^C" << std::endl;
                Interrupt::clear();
                continue;
            }
            break;
        }
        if (line == "exit" || line == "quit") break;
        // If interrupted during typing, drop the line and continue
        if (s_interrupted.exchange(false)) { out_ << "^C" << std::endl; Interrupt::clear(); continue; }

        // Whole-line comments and shebang are ignored
        {
            auto t = trim(line);
            if (!t.empty() && (t[0] == '#' || (t.size() >= 2 && t[0] == '#' && t[1] == '!'))) {
                (void)s_interrupted.exchange(false);
                continue;
            }
        }
        execute_line_with_env(line, env_);
        // If interrupted during command execution, just clear the flags
        (void)s_interrupted.exchange(false);
        Interrupt::clear();
    }

#ifndef _WIN32
    // Restore previous handler
    std::signal(SIGINT, prev_handler);
#endif
    return 0;
}

std::string Shell::prompt_user() const {
    std::string u = env_.get("USER");
    if (u.empty()) u = "user";
    return u;
}

std::string Shell::prompt_path_display() const {
    auto p = cwd_.generic_string();
    if (p == "/") return std::string("~");
    return p;
}

int Shell::execute_script_file(const std::filesystem::path& host_path, bool source_mode, Environment& base_env, const std::vector<std::string>& args) {
    std::string data;
    try {
        data = vfs_.readFile(host_path);
    } catch (const std::exception& e) {
        out_ << "sh: cannot open: " << e.what() << std::endl; return 1;
    }
    std::istringstream is(data);
    std::string line;
    int last_rc = 0;
    // Use a temporary env for direct execution to avoid persisting variables
    Environment* env_ptr = &base_env;
    Environment temp_env;
    if (!source_mode) {
        temp_env = base_env;
        env_ptr = &temp_env;
    }
    // Initialize positional parameters and $? for direct script execution
    if (!source_mode) {
        try {
            std::error_code ec;
            auto rel = std::filesystem::relative(host_path, vfs_.root(), ec);
            auto vpath = (std::filesystem::path("/") / rel).generic_string();
            env_ptr->set("0", vpath);
        } catch (...) {
            env_ptr->set("0", host_path.generic_string());
        }
        env_ptr->set("#", std::to_string(args.size()));
        for (size_t i = 0; i < args.size(); ++i) env_ptr->set(std::to_string(i+1), args[i]);
        if (env_ptr->get("?").empty()) env_ptr->set("?", "0");
    }

    struct IfFrame { bool executing; bool taken; };
    std::vector<IfFrame> stack;
    auto should_run = [&](){ for (const auto& f : stack) if (!f.executing) return false; return true; };
    auto ancestors_run = [&](){ if (stack.empty()) return true; for (size_t i=0;i+1<stack.size();++i) if (!stack[i].executing) return false; return true; };

    while (std::getline(is, line)) {
        auto t = trim(line);
        if (t.empty()) continue;
        if (t[0] == '#') continue; // ignore comments and shebang

        if (t.rfind("if ", 0) == 0) {
            std::string rest = t.substr(3);
            auto pos_then = rest.find(" then");
            if (pos_then == std::string::npos) { out_ << "sh: syntax: expected 'then' on same line" << std::endl; return 2; }
            std::string cond = rtrim(rest.substr(0, pos_then));
            std::string after_then = rest.substr(pos_then + 5); // skip " then"
            // trim leading spaces
            after_then = ltrim(after_then);
            if (!cond.empty() && cond.back() == ';') cond.pop_back();
            bool exec_now = false;
            if (should_run()) {
                int rc = execute_line_with_env(cond, *env_ptr);
                env_ptr->set("?", std::to_string(rc));
                exec_now = (rc == 0);
                last_rc = rc;
            }
            // executing for this frame depends on both parent allowance and this condition
            bool frame_exec = should_run() && exec_now;
            stack.push_back(IfFrame{frame_exec, exec_now});

            // Handle inline commands after 'then' on the same line (e.g., "then echo hi; fi")
            if (!after_then.empty()) {
                // Split by ';'
                size_t start = 0;
                while (start <= after_then.size()) {
                    size_t semi = after_then.find(';', start);
                    std::string seg = (semi == std::string::npos) ? after_then.substr(start)
                                                                  : after_then.substr(start, semi - start);
                    seg = trim(seg);
                    if (!seg.empty()) {
                        if (seg == "fi") { if (!stack.empty()) stack.pop_back(); break; }
                        if (seg == "else" || seg.rfind("elif ", 0) == 0) {
                            // Inline else/elif not supported in MVP; stop and let subsequent lines handle
                            break;
                        }
                        if (should_run()) {
                            int rc2 = execute_line_with_env(seg, *env_ptr);
                            env_ptr->set("?", std::to_string(rc2));
                            last_rc = rc2;
                        }
                    }
                    if (semi == std::string::npos) break;
                    start = semi + 1;
                }
            }
            continue;
        }
        if (t.rfind("elif ", 0) == 0) {
            if (stack.empty()) { out_ << "sh: 'elif' without matching 'if'" << std::endl; return 2; }
            bool parent_ok = ancestors_run();
            bool exec_now = false;
            if (parent_ok && !stack.back().taken) {
                std::string cond = t.substr(5);
                auto pos_then = cond.find(" then");
                if (pos_then == std::string::npos) { out_ << "sh: syntax: expected 'then' after elif" << std::endl; return 2; }
                cond = rtrim(cond.substr(0, pos_then));
                if (!cond.empty() && cond.back() == ';') cond.pop_back();
                int rc = execute_line_with_env(cond, *env_ptr);
                env_ptr->set("?", std::to_string(rc));
                exec_now = (rc == 0);
                last_rc = rc;
            }
            stack.back().executing = parent_ok && !stack.back().taken && exec_now;
            stack.back().taken = stack.back().taken || stack.back().executing;
            continue;
        }
        if (t == "else") {
            if (stack.empty()) { out_ << "sh: 'else' without matching 'if'" << std::endl; return 2; }
            bool parent_ok = ancestors_run();
            bool exec_now = parent_ok && !stack.back().taken;
            stack.back().executing = exec_now;
            stack.back().taken = stack.back().taken || exec_now;
            continue;
        }
        if (t == "fi") {
            if (stack.empty()) { out_ << "sh: 'fi' without matching 'if'" << std::endl; return 2; }
            stack.pop_back();
            continue;
        }

        if (should_run()) {
            last_rc = execute_line_with_env(line, *env_ptr);
            env_ptr->set("?", std::to_string(last_rc));
        } else {
            // skipped branch
        }
    }
    return last_rc;
}
