#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Cat : public ICommand {
public:
    std::string name() const override { return "cat"; }
    std::string help() const override {
        return R"(cat: concatenate and print files
Synopsis:
  cat [file]
Notes:
  When no file is provided, reads from standard input.
Examples:
  cat a.txt
  cat < a.txt
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) {
            std::string line;
            // Read all from stdin until EOF
            while (std::getline(ctx.in, line)) {
                ctx.out << line;
                if (!ctx.in.eof()) ctx.out << '\n';
            }
            return 0;
        } else {
            try {
                auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[1]));
                ctx.out << ctx.vfs.readFile(abs);
                return 0;
            } catch (const std::exception& e) {
                ctx.out << "cat: " << e.what() << std::endl;
                return 1;
            }
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_cat(){ return std::make_unique<Cat>(); } }
