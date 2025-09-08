#pragma once
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <filesystem>

class IVfs;
class Environment;

class CommandContext {
public:
    CommandContext(const std::vector<std::string>& args,
                   std::istream& in,
                   std::ostream& out,
                   IVfs& vfs,
                   Environment& env,
                   std::filesystem::path& cwd)
        : args(args), in(in), out(out), vfs(vfs), env(env), cwd(cwd) {}

    const std::vector<std::string>& args;
    std::istream& in;
    std::ostream& out;
    IVfs& vfs;
    Environment& env;
    std::filesystem::path& cwd; // VFS-internal cwd (absolute inside VFS, like /home/user)
};

