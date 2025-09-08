#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Mkdir : public ICommand {
public:
    std::string name() const override { return "mkdir"; }
    std::string help() const override {
        return R"(mkdir: create directories
Synopsis:
  mkdir [-p] <dir>
Options:
  -p   Make parent directories as needed (no error if existing)
Examples:
  mkdir demo
  mkdir -p projects/demo/src
)";
    }
    int execute(CommandContext& ctx) override {
        bool recursive = false;
        size_t idx = 1;
        if (idx < ctx.args.size() && ctx.args[idx] == "-p") { recursive = true; ++idx; }
        if (idx >= ctx.args.size()) { ctx.out << "mkdir: missing operand" << std::endl; return 2; }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[idx]));
            ctx.vfs.mkdir(abs, recursive);
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "mkdir: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_mkdir(){ return std::make_unique<Mkdir>(); } }
