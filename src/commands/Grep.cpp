#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>

static std::string to_lower(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

class Grep : public ICommand {
public:
    std::string name() const override { return "grep"; }
    std::string help() const override {
        return R"(grep: print lines matching a pattern
Synopsis:
  grep [-n] [-i] [-r] PATTERN [path]
Options:
  -n   Prefix each line with line number
  -i   Ignore case distinctions
  -r   Read all files under each directory, recursively
Notes:
  Without a path, reads from standard input.
Examples:
  grep -n error app.log
  grep -ri todo /projects
)";
    }
    int execute(CommandContext& ctx) override {
        using namespace std;
        namespace fs = std::filesystem;
        bool opt_n=false, opt_i=false, opt_r=false;
        string pattern;
        vector<string> paths;
        for (size_t i=1;i<ctx.args.size();++i){
            const auto& a=ctx.args[i];
            if (a=="-n") { opt_n=true; continue; }
            if (a=="-i") { opt_i=true; continue; }
            if (a=="-r") { opt_r=true; continue; }
            if (pattern.empty()) { pattern=a; continue; }
            paths.push_back(a);
        }
        if (pattern.empty()) { ctx.out << "grep: missing PATTERN" << endl; return 2; }
        string pat = opt_i ? to_lower(pattern) : pattern;

        auto search_file = [&](const fs::path& host_path, const fs::path& vfs_base){
            try{
                string data = ctx.vfs.readFile(host_path);
                // iterate lines
                size_t pos=0; size_t line_no=1; bool first=true;
                while (pos <= data.size()){
                    size_t end = data.find('\n', pos);
                    string line = end==string::npos ? data.substr(pos) : data.substr(pos, end-pos);
                    string hay = opt_i ? to_lower(line) : line;
                    if (hay.find(pat) != string::npos){
                        std::error_code ec;
                        auto rel = std::filesystem::relative(host_path, ctx.vfs.root(), ec);
                        fs::path v = fs::path("/") / rel;
                        ctx.out << v.generic_string();
                        ctx.out << ':';
                        if (opt_n) ctx.out << line_no << ':';
                        ctx.out << line << '\n';
                    }
                    if (end==string::npos) break; else { pos = end+1; ++line_no; }
                }
            }catch(const std::exception& e){ ctx.out << "grep: " << e.what() << endl; }
        };

        if (paths.empty()){
            // read from stdin
            string all, line; vector<string> lines; lines.reserve(128);
            while (std::getline(ctx.in, line)) { lines.push_back(line); }
            for (size_t i=0;i<lines.size();++i){
                string hay = opt_i ? to_lower(lines[i]) : lines[i];
                if (hay.find(pat) != string::npos){
                    if (opt_n) ctx.out << (i+1) << ':';
                    ctx.out << lines[i] << '\n';
                }
            }
            return 0;
        }

        for (auto& pstr : paths){
            fs::path vfs_p = to_vfs_path(pstr);
            fs::path host;
            try { host = ctx.vfs.resolveSecure(ctx.cwd, vfs_p); }
            catch(const std::exception& e){ ctx.out << "grep: " << e.what() << endl; continue; }

            std::error_code ec;
            if (fs::is_directory(host, ec)){
                if (!opt_r){ ctx.out << "grep: " << pstr << ": Is a directory (use -r)" << endl; continue; }
                for (fs::recursive_directory_iterator it(host, fs::directory_options::skip_permission_denied, ec), end; it!=end; ++it){
                    if (it->is_regular_file(ec)) search_file(it->path(), vfs_p);
                }
            } else if (fs::is_regular_file(host, ec)){
                search_file(host, vfs_p);
            } else {
                ctx.out << "grep: cannot access: " << pstr << endl;
            }
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_grep(){ return std::make_unique<Grep>(); } }

