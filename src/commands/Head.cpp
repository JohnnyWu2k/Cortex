#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <sstream>

class Head : public ICommand {
public:
    std::string name() const override { return "head"; }
    std::string help() const override {
        return R"(head: output the first part of files
Synopsis:
  head [-n N] [file]
Options:
  -n N   Print the first N lines (default 10)
Notes:
  Without a file, reads from standard input.
Examples:
  head -n 5 a.txt
  cat a.txt | head
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() == 1) {
            ctx.out << "head: common usage\n  head [-n N] [file]\n  cat file | head\nUse 'help head' for full help." << std::endl;
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
                ctx.out << "head: " << e.what() << std::endl; return 1;
            }
        }

        std::string line;
        size_t count = 0;
        while (count < n && std::getline(*in, line)) {
            ctx.out << line;
            if (!in->eof()) ctx.out << '\n';
            ++count;
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_head(){ return std::make_unique<Head>(); } }
