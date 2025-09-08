#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <chrono>

class Stat : public ICommand {
public:
    std::string name() const override { return "stat"; }
    std::string help() const override {
        return R"(stat: display file status
Synopsis:
  stat <path>
Output:
  name=<name> size=<bytes> type=<file|dir>
Examples:
  stat a.txt
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) { ctx.out << "stat: missing path" << std::endl; return 2; }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[1]));
            auto s = ctx.vfs.stat(abs);
            ctx.out << "name=" << s.name
                    << " size=" << s.size
                    << " type=" << (s.is_dir ? "dir" : "file")
                    << std::endl;
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "stat: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_stat(){ return std::make_unique<Stat>(); } }
