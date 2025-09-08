#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ICommand.hpp"

class CommandRegistry {
public:
    void add(std::unique_ptr<ICommand> cmd);
    ICommand* find(const std::string& name);
    std::vector<std::string> list() const;
private:
    std::map<std::string, std::unique_ptr<ICommand>> commands_;
};

