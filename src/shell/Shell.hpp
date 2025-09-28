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
    int execute_line_with_env(const std::string& line, Environment& env);
    int execute_script_file(const std::filesystem::path& host_path,
                            bool source_mode,
                            Environment& base_env,
                            const std::vector<std::string>& args = {});
    bool has_exec_permission(const std::filesystem::path& host_path) const;
    static std::string expand_vars(const std::string& input, const Environment& env);
    static std::string ltrim(const std::string& s);
    static std::string rtrim(const std::string& s);
    static std::string trim(const std::string& s);
    std::string prompt_path_display() const;
    std::string prompt_user() const;
};
