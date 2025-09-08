#include "CommandRegistry.hpp"

void CommandRegistry::add(std::unique_ptr<ICommand> cmd) {
    auto key = cmd->name();
    commands_[std::move(key)] = std::move(cmd);
}

ICommand* CommandRegistry::find(const std::string& name) {
    auto it = commands_.find(name);
    if (it == commands_.end()) return nullptr;
    return it->second.get();
}

std::vector<std::string> CommandRegistry::list() const {
    std::vector<std::string> names;
    names.reserve(commands_.size());
    for (auto& kv : commands_) names.push_back(kv.first);
    return names;
}

