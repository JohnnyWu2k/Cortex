#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../shell/CommandRegistry.hpp"

extern CommandRegistry* g_registry_for_help;

class Help : public ICommand {
public:
    std::string name() const override { return "help"; }
    std::string help() const override {
        return R"(help: show help for commands
Synopsis:
  help [command]
Notes:
  With no arguments, lists available commands. With a command name,
  shows that command's usage, options, and examples.
Examples:
  help
  help ls
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() == 1) {
            ctx.out << "Commands:" << std::endl;
            if (!g_registry_for_help) return 0;
            for (auto& n : g_registry_for_help->list()) ctx.out << "  " << n << std::endl;
            ctx.out << "Use 'help <cmd>' for details." << std::endl;
            return 0;
        }
        if (!g_registry_for_help) return 0;
        auto* cmd = g_registry_for_help->find(ctx.args[1]);
        if (!cmd) { ctx.out << "help: unknown command: " << ctx.args[1] << std::endl; return 1; }
        ctx.out << cmd->help() << std::endl;
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_help(){ return std::make_unique<Help>(); } }
