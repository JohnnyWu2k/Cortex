#pragma once
#include <istream>
#include <ostream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "CommandRegistry.hpp"

class IVfs;
class Environment;

class Shell {
public:
    Shell(std::istream& in, std::ostream& out, IVfs& vfs, Environment& env);
    int run();
private:
    std::istream& in_;
    std::ostream& out_;
    IVfs& vfs_;
    Environment& env_;
    std::filesystem::path cwd_; // VFS absolute path (e.g., /home/user)
    CommandRegistry registry_;

    void register_builtin_commands();
    int execute_line(const std::string& line);
    std::string prompt_path_display() const;
    std::string prompt_user() const;
};
