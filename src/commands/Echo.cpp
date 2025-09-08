#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"

class Echo : public ICommand {
public:
    std::string name() const override { return "echo"; }
    std::string help() const override {
        return R"(echo: write arguments to standard output
Synopsis:
  echo [args...]
Examples:
  echo hello world
)";
    }
    int execute(CommandContext& ctx) override {
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            if (i > 1) ctx.out << ' ';
            ctx.out << ctx.args[i];
        }
        ctx.out << std::endl;
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_echo(){ return std::make_unique<Echo>(); } }
