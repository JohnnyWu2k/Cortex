#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Cp : public ICommand {
public:
    std::string name() const override { return "cp"; }
    std::string help() const override {
        return R"(cp: copy files and directories
Synopsis:
  cp [-r] <src> <dst>
Options:
  -r   Copy directories recursively
Notes:
  Overwrites existing files.
Examples:
  cp a.txt b.txt
  cp -r dir1 dir2
)";
    }
    int execute(CommandContext& ctx) override {
        bool recursive = false;
        size_t idx = 1;
        if (idx < ctx.args.size() && ctx.args[idx] == "-r") { recursive = true; ++idx; }
        if (idx + 1 >= ctx.args.size()) { ctx.out << "cp: missing operand" << std::endl; return 2; }
        try {
            auto src = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[idx]));
            auto dst = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[idx+1]));
            ctx.vfs.copy(src, dst, recursive);
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "cp: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_cp(){ return std::make_unique<Cp>(); } }
