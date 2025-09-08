#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../core/Environment.hpp"

class EnvCmd : public ICommand {
public:
    std::string name() const override { return "env"; }
    std::string help() const override {
        return R"(env: print shell environment variables
Synopsis:
  env
Examples:
  env
)";
    }
    int execute(CommandContext& ctx) override {
        for (auto& kv : ctx.env.list()) {
            ctx.out << kv.first << "=" << kv.second << std::endl;
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_env(){ return std::make_unique<EnvCmd>(); } }
