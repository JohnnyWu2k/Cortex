#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"

class Pwd : public ICommand {
public:
    std::string name() const override { return "pwd"; }
    std::string help() const override {
        return R"(pwd: print the current working directory
Synopsis:
  pwd
Examples:
  pwd
)";
    }
    int execute(CommandContext& ctx) override {
        ctx.out << ctx.cwd.generic_string() << std::endl;
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_pwd(){ return std::make_unique<Pwd>(); } }
