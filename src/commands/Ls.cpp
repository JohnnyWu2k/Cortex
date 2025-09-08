#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"

class Ls : public ICommand {
public:
    std::string name() const override { return "ls"; }
    std::string help() const override {
        return R"(ls: list directory contents
Synopsis:
  ls [-l] [-a] [path]
Options:
  -l   Use a long listing format (type/size/name)
  -a   Include entries starting with '.'
Notes:
  Only a single [path] is supported in MVP.
Examples:
  ls
  ls -la /etc
)";
    }
    int execute(CommandContext& ctx) override {
        bool opt_l = false;
        bool opt_a = false;
        std::filesystem::path target{"."};
        // parse flags and optional single path
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            const auto& a = ctx.args[i];
            if (!a.empty() && a[0] == '-') {
                // allow combined flags, e.g., -la
                for (size_t j = 1; j < a.size(); ++j) {
                    if (a[j] == 'l') opt_l = true;
                    else if (a[j] == 'a') opt_a = true;
                    else { ctx.out << "ls: unknown option -" << a[j] << std::endl; return 2; }
                }
            } else {
                target = to_vfs_path(a);
            }
        }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, target);
            auto entries = ctx.vfs.list(abs);
            for (auto& e : entries) {
                if (!opt_a && !e.name.empty() && e.name[0] == '.') continue;
                if (opt_l) {
                    ctx.out << (e.is_dir ? 'd' : '-') << ' ' << e.size << ' ' << e.name;
                } else {
                    ctx.out << e.name;
                    if (e.is_dir) ctx.out << "/";
                }
                ctx.out << std::endl;
            }
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "ls: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_ls(){ return std::make_unique<Ls>(); } }
