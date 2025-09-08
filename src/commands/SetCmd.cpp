#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../core/Environment.hpp"

class SetCmd : public ICommand {
public:
    std::string name() const override { return "set"; }
    std::string help() const override {
        return R"(set: set an environment variable
Synopsis:
  set KEY=VALUE
Examples:
  set USER=alice
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) { ctx.out << "set: missing KEY=VALUE" << std::endl; return 2; }
        auto pos = ctx.args[1].find('=');
        if (pos == std::string::npos) { ctx.out << "set: format KEY=VALUE" << std::endl; return 2; }
        ctx.env.set(ctx.args[1].substr(0, pos), ctx.args[1].substr(pos + 1));
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_set(){ return std::make_unique<SetCmd>(); } }
