#include <iostream>
#include <filesystem>
#include <string>

#include "core/Environment.hpp"
#include "vfs/FolderVfs.hpp"
#include "shell/Shell.hpp"

static std::filesystem::path default_root(bool portable) {
    namespace fs = std::filesystem;
    if (portable) {
        return fs::current_path() / "data" / "rootfs";
    }
#ifdef _WIN32
    const char* localAppData = std::getenv("LOCALAPPDATA");
    fs::path base = localAppData ? fs::path(localAppData) : fs::temp_directory_path();
    return base / "Cortex" / "rootfs";
#else
    const char* home = std::getenv("HOME");
    fs::path base = home ? fs::path(home) : fs::temp_directory_path();
    return base / ".local" / "share" / "Cortex" / "rootfs";
#endif
}

namespace Builtins {
    std::unique_ptr<ICommand> make_pwd();
    std::unique_ptr<ICommand> make_cd();
    std::unique_ptr<ICommand> make_ls();
    std::unique_ptr<ICommand> make_echo();
    std::unique_ptr<ICommand> make_mkdir();
    std::unique_ptr<ICommand> make_touch();
    std::unique_ptr<ICommand> make_cat();
    std::unique_ptr<ICommand> make_rm();
    std::unique_ptr<ICommand> make_cp();
    std::unique_ptr<ICommand> make_mv();
    std::unique_ptr<ICommand> make_env();
    std::unique_ptr<ICommand> make_set();
    std::unique_ptr<ICommand> make_unset();
    std::unique_ptr<ICommand> make_help();
    std::unique_ptr<ICommand> make_version();
    std::unique_ptr<ICommand> make_stat();
    std::unique_ptr<ICommand> make_clear();
    std::unique_ptr<ICommand> make_head();
    std::unique_ptr<ICommand> make_tail();
    std::unique_ptr<ICommand> make_find();
    std::unique_ptr<ICommand> make_grep();
    std::unique_ptr<ICommand> make_pack();
    std::unique_ptr<ICommand> make_unpack();
    std::unique_ptr<ICommand> make_chmod();
    std::unique_ptr<ICommand> make_test();
    std::unique_ptr<ICommand> make_bracket();
    std::unique_ptr<ICommand> make_pkg();
}

namespace Builtins {
    void register_all(CommandRegistry& reg) {
        reg.add(make_pwd());
        reg.add(make_cd());
        reg.add(make_ls());
        reg.add(make_echo());
        reg.add(make_mkdir());
        reg.add(make_touch());
        reg.add(make_cat());
        reg.add(make_rm());
        reg.add(make_cp());
        reg.add(make_mv());
        reg.add(make_env());
        reg.add(make_set());
        reg.add(make_unset());
        reg.add(make_help());
        reg.add(make_version());
        reg.add(make_stat());
        reg.add(make_clear());
        reg.add(make_head());
        reg.add(make_tail());
        reg.add(make_find());
        reg.add(make_grep());
        reg.add(make_pack());
        reg.add(make_unpack());
        reg.add(make_chmod());
        reg.add(make_test());
        reg.add(make_bracket());
        reg.add(make_pkg());
    }
}

int main(int argc, char** argv) {
    bool portable = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--portable") portable = true;
    }

    Environment env;

    FolderVfs vfs(default_root(portable));

    Shell shell(std::cin, std::cout, vfs, env);
    return shell.run();
}
