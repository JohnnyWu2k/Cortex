#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"

class Version : public ICommand {
public:
    std::string name() const override { return "version"; }
    std::string help() const override {
        return R"(version: display cortex version and VFS info
Synopsis:
  version
Examples:
  version
)";
    }
    int execute(CommandContext& ctx) override {
        ctx.out << "cortex v0.1 (FolderVfs) root=" << ctx.vfs.root().string() << std::endl;
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_version(){ return std::make_unique<Version>(); } }
