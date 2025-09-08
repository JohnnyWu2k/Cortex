#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <filesystem>
#include <system_error>

static bool match_glob(const std::string& name, const std::string& pat) {
    // Very simple glob: * and ? only, no character classes
    size_t n = 0, p = 0, star = std::string::npos, match = 0;
    while (n < name.size()) {
        if (p < pat.size() && (pat[p] == '?' || pat[p] == name[n])) { ++n; ++p; }
        else if (p < pat.size() && pat[p] == '*') { star = p++; match = n; }
        else if (star != std::string::npos) { p = star + 1; n = ++match; }
        else return false;
    }
    while (p < pat.size() && pat[p] == '*') ++p;
    return p == pat.size();
}

class Find : public ICommand {
public:
    std::string name() const override { return "find"; }
    std::string help() const override {
        return R"(find: search for files in a directory hierarchy
Synopsis:
  find <path> [-name PAT] [-type f|d] [-size +N|-N|N] [-maxdepth D]
Options:
  -name PAT     Filter by glob pattern on basename (* and ? supported)
  -type f|d     Filter by type: f=file, d=directory
  -size +/-N|N  File size in bytes: + greater than, - less than, exact otherwise
  -maxdepth D   Descend at most D levels (0 means only the start path)
Notes:
  Symlinks are not followed for recursion in MVP.
Examples:
  find . -name "*.txt" -maxdepth 1
  find /projects -type f -size +1024
)";
    }
    int execute(CommandContext& ctx) override {
        namespace fs = std::filesystem;
        fs::path start{"."};
        std::string name_pat;
        char type_filter = 0; // 0, 'f', 'd'
        long long size_filter = LLONG_MIN; // use sentinel
        int size_mode = 0; // -1: <, 0: ==, +1: >
        int maxdepth = INT_MAX;

        size_t i = 1;
        if (i < ctx.args.size() && ctx.args[i].rfind("-", 0) != 0) {
            start = to_vfs_path(ctx.args[i++]);
        }
        for (; i < ctx.args.size(); ++i) {
            const auto& a = ctx.args[i];
            if (a == "-name" && i + 1 < ctx.args.size()) { name_pat = ctx.args[++i]; continue; }
            if (a == "-type" && i + 1 < ctx.args.size()) { char t = ctx.args[++i][0]; if (t=='f'||t=='d') type_filter=t; continue; }
            if (a == "-size" && i + 1 < ctx.args.size()) {
                std::string s = ctx.args[++i];
                if (!s.empty() && (s[0] == '+' || s[0] == '-')) { size_mode = (s[0] == '+') ? +1 : -1; s = s.substr(1); }
                size_filter = std::stoll(s);
                continue;
            }
            if (a == "-maxdepth" && i + 1 < ctx.args.size()) { maxdepth = std::stoi(ctx.args[++i]); continue; }
            ctx.out << "find: unknown or malformed option: " << a << std::endl; return 2;
        }

        fs::path start_abs;
        try { start_abs = ctx.vfs.resolveSecure(ctx.cwd, start); }
        catch (const std::exception& e) { ctx.out << "find: " << e.what() << std::endl; return 1; }

        auto print_vfs_path = [&](const fs::path& host) {
            std::error_code ec;
            auto rel = fs::relative(host, ctx.vfs.root(), ec);
            fs::path v = fs::path("/") / rel;
            ctx.out << v.generic_string() << '\n';
        };

        auto match_entry = [&](const fs::directory_entry& de) {
            std::error_code ec;
            bool isdir = de.is_directory(ec);
            bool isfile = de.is_regular_file(ec);
            if (type_filter == 'd' && !isdir) return false;
            if (type_filter == 'f' && !isfile) return false;
            if (!name_pat.empty() && !match_glob(de.path().filename().string(), name_pat)) return false;
            if (size_filter != LLONG_MIN && isfile) {
                auto sz = (long long)fs::file_size(de.path(), ec);
                if (size_mode < 0 && !(sz < size_filter)) return false;
                if (size_mode == 0 && !(sz == size_filter)) return false;
                if (size_mode > 0 && !(sz > size_filter)) return false;
            }
            return true;
        };

        std::error_code ec;
        fs::file_status st = fs::status(start_abs, ec);
        if (ec) { ctx.out << "find: cannot access start path" << std::endl; return 1; }

        if (fs::is_regular_file(st)) {
            fs::directory_entry de(start_abs, ec);
            if (!ec && match_entry(de)) print_vfs_path(start_abs);
            return 0;
        }

        // Include start directory itself when depth 0 and matches filters
        if (fs::is_directory(st)) {
            fs::directory_entry de(start_abs, ec);
            if (!ec && match_entry(de)) print_vfs_path(start_abs);

            fs::recursive_directory_iterator it(start_abs, fs::directory_options::skip_permission_denied, ec), end;
            for (; it != end; ++it) {
                if ((int)it.depth() >= maxdepth) { it.disable_recursion_pending(); }
                const auto& de2 = *it;
                if (match_entry(de2)) print_vfs_path(de2.path());
            }
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_find(){ return std::make_unique<Find>(); } }

