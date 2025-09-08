#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../core/Environment.hpp"

class UnsetCmd : public ICommand {
public:
    std::string name() const override { return "unset"; }
    std::string help() const override {
        return R"(unset: remove an environment variable
Synopsis:
  unset KEY
Examples:
  unset USER
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) { ctx.out << "unset: missing KEY" << std::endl; return 2; }
        ctx.env.unset(ctx.args[1]);
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_unset(){ return std::make_unique<UnsetCmd>(); } }
