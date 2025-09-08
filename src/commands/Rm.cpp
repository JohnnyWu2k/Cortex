#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Rm : public ICommand {
public:
    std::string name() const override { return "rm"; }
    std::string help() const override {
        return R"(rm: remove files or directories
Synopsis:
  rm [-r] <path>
Options:
  -r   Remove directories and their contents recursively
Notes:
  Non-recursive remove fails if <path> is a directory.
Examples:
  rm file.txt
  rm -r old_project
)";
    }
    int execute(CommandContext& ctx) override {
        bool recursive = false;
        size_t idx = 1;
        if (idx < ctx.args.size() && ctx.args[idx] == "-r") { recursive = true; ++idx; }
        if (idx >= ctx.args.size()) { ctx.out << "rm: missing operand" << std::endl; return 2; }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[idx]));
            ctx.vfs.remove(abs, recursive);
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "rm: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_rm(){ return std::make_unique<Rm>(); } }
