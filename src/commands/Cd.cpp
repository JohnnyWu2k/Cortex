#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Cd : public ICommand {
public:
    std::string name() const override { return "cd"; }
    std::string help() const override {
        return R"(cd: change the working directory
Synopsis:
  cd [dir]
Notes:
  Without arguments, changes to '/'. Accepts absolute or relative VFS paths.
Examples:
  cd /projects/demo
  cd ..
)";
    }
    int execute(CommandContext& ctx) override {
        std::filesystem::path target = ctx.args.size() > 1 ? to_vfs_path(ctx.args[1]) : std::filesystem::path("/");
        try {
            auto resolved = ctx.vfs.resolveSecure(ctx.cwd, target);
            std::error_code ec;
            if (!std::filesystem::is_directory(resolved, ec)) {
                ctx.out << "cd: not a directory: " << target.string() << std::endl;
                return 1;
            }
            // compute VFS-absolute path from host path
            auto rel = std::filesystem::relative(resolved, ctx.vfs.root(), ec);
            ctx.cwd = std::filesystem::path("/") / rel;
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "cd: " << e.what() << std::endl;
            return 13;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_cd(){ return std::make_unique<Cd>(); } }
