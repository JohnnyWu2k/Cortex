#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <deque>
#include <sstream>

class Tail : public ICommand {
public:
    std::string name() const override { return "tail"; }
    std::string help() const override {
        return R"(tail: output the last part of files
Synopsis:
  tail [-n N] [file]
Options:
  -n N   Print the last N lines (default 10)
Notes:
  Without a file, reads from standard input.
Examples:
  tail -n 20 a.txt
  cat a.txt | tail
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() == 1) {
            ctx.out << "tail: common usage\n  tail [-n N] [file]\n  cat file | tail\nUse 'help tail' for full help." << std::endl;
            return 0;
        }
        size_t n = 10;
        std::string file;
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            const auto& a = ctx.args[i];
            if (a == "-n" && i + 1 < ctx.args.size()) { n = static_cast<size_t>(std::stoul(ctx.args[++i])); continue; }
            if (a.rfind("-n", 0) == 0 && a.size() > 2) { n = static_cast<size_t>(std::stoul(a.substr(2))); continue; }
            file = a;
        }

        std::istringstream inbuf;
        std::istream* in = &ctx.in;
        std::string data;
        if (!file.empty()) {
            try {
                auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(file));
                data = ctx.vfs.readFile(abs);
                inbuf.str(data);
                in = &inbuf;
            } catch (const std::exception& e) {
                ctx.out << "tail: " << e.what() << std::endl; return 1;
            }
        }

        std::deque<std::string> q;
        std::string line;
        while (std::getline(*in, line)) {
            q.push_back(line);
            if (q.size() > n) q.pop_front();
        }
        bool first = true;
        for (auto& s : q) {
            if (!first) ctx.out << '\n';
            ctx.out << s;
            first = false;
        }
        if (!q.empty()) ctx.out << '\n';
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_tail(){ return std::make_unique<Tail>(); } }
