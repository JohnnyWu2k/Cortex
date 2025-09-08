#include "Environment.hpp"

std::string Environment::get(const std::string& key) const {
    auto it = kv_.find(key);
    if (it == kv_.end()) return std::string();
    return it->second;
}

void Environment::set(const std::string& key, const std::string& value) {
    kv_[key] = value;
}

void Environment::unset(const std::string& key) {
    kv_.erase(key);
}

std::vector<std::pair<std::string, std::string>> Environment::list() const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(kv_.size());
    for (auto& kv : kv_) out.emplace_back(kv.first, kv.second);
    return out;
}

