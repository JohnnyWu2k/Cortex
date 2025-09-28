#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include "../util/ExecDb.hpp"

class Chmod : public ICommand {
public:
    std::string name() const override { return "chmod"; }
    std::string help() const override {
        return R"(chmod: set or clear execute permission (MVP)
Synopsis:
  chmod +x <path>
  chmod -x <path>
Notes:
  Only the execute bit is tracked in MVP.)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 3) { ctx.out << "chmod: usage: chmod [+x|-x] <path>" << std::endl; return 2; }
        std::string flag = ctx.args[1];
        if (flag != "+x" && flag != "-x") { ctx.out << "chmod: support only +x or -x" << std::endl; return 2; }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[2]));
            // validate file exists
            auto st = ctx.vfs.stat(abs);
            if (st.is_dir) { ctx.out << "chmod: not a file" << std::endl; return 2; }
            execdb::set(ctx.vfs, abs, flag == "+x");
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "chmod: " << e.what() << std::endl; return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_chmod(){ return std::make_unique<Chmod>(); } }
