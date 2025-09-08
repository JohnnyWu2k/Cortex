#pragma once
#include <string>

class CommandContext;

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string name() const = 0;
    virtual std::string help() const = 0;
    virtual int execute(CommandContext& context) = 0;
};

