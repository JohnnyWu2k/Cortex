#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Touch : public ICommand {
public:
    std::string name() const override { return "touch"; }
    std::string help() const override {
        return R"(touch: create file or update its timestamp
Synopsis:
  touch <file>
Examples:
  touch a.txt
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) { ctx.out << "touch: missing file" << std::endl; return 2; }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[1]));
            ctx.vfs.touch(abs);
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "touch: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_touch(){ return std::make_unique<Touch>(); } }
