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

// Forward declare factory to register commands
namespace Builtins { void register_all(CommandRegistry& reg); }
// Expose registry for Help command
CommandRegistry* g_registry_for_help = nullptr;

// SIGINT (Ctrl+C) handling
static std::atomic<bool> s_interrupted{false};
static void on_sigint(int) { s_interrupted.store(true); }

#ifdef _WIN32
static BOOL WINAPI on_console_ctrl(DWORD type) {
    if (type == CTRL_C_EVENT) {
        s_interrupted.store(true);
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

int Shell::execute_line(const std::string& line) {
    auto tokens = Parser::split(line);
    if (tokens.empty()) return 0;

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

        CommandContext ctx(args, *current_in, *out_stream, vfs_, env_, cwd_);
        int rc = cmd->execute(ctx);
        if (rc != 0) return rc;

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
                continue;
            }
            break;
        }
        if (line == "exit" || line == "quit") break;
        // If interrupted during typing, drop the line and continue
        if (s_interrupted.exchange(false)) continue;

        execute_line(line);
        // If interrupted during command execution, just clear the flag
        (void)s_interrupted.exchange(false);
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
