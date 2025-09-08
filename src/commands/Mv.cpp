#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Mv : public ICommand {
public:
    std::string name() const override { return "mv"; }
    std::string help() const override {
        return R"(mv: move or rename files
Synopsis:
  mv <src> <dst>
Notes:
  Overwrites existing files.
Examples:
  mv a.txt b.txt
  mv dir1 dir2
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 3) { ctx.out << "mv: missing operand" << std::endl; return 2; }
        try {
            auto src = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[1]));
            auto dst = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[2]));
            ctx.vfs.move(src, dst);
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "mv: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_mv(){ return std::make_unique<Mv>(); } }
