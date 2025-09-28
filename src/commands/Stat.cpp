#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <chrono>
#include <sstream>

class Stat : public ICommand {
public:
    std::string name() const override { return "stat"; }
    std::string help() const override {
        return R"(stat: display file status
Synopsis:
  stat <path>
Output:
  name=<name> size=<bytes> type=<file|dir>
Examples:
  stat a.txt
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) { ctx.out << "stat: missing path" << std::endl; return 2; }
        try {
            auto abs = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(ctx.args[1]));
            auto s = ctx.vfs.stat(abs);
            // MVP permission model: only exec bit tracked via /etc/execdb
            std::string perms = "rw-";
            try {
                auto execdb = ctx.vfs.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/etc/execdb"));
                auto data = ctx.vfs.readFile(execdb);
                std::istringstream is(data);
                std::string line;
                auto target = abs.generic_string();
                while (std::getline(is, line)) {
                    // trim
                    size_t i=0; while (i<line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
                    size_t j=line.size(); while (j>i && std::isspace(static_cast<unsigned char>(line[j-1]))) --j;
                    if (j>i && std::string(line.begin()+i, line.begin()+j) == target) { perms = "rwx"; break; }
                }
            } catch (const std::exception&) { /* no db -> default rw- */ }
            ctx.out << "name=" << s.name
                    << " size=" << s.size
                    << " type=" << (s.is_dir ? "dir" : "file")
                    << " perms=" << perms
                    << std::endl;
            return 0;
        } catch (const std::exception& e) {
            ctx.out << "stat: " << e.what() << std::endl;
            return 1;
        }
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_stat(){ return std::make_unique<Stat>(); } }
