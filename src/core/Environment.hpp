#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class Environment {
public:
    std::string get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    void unset(const std::string& key);
    std::vector<std::pair<std::string, std::string>> list() const;
private:
    std::unordered_map<std::string, std::string> kv_;
};

