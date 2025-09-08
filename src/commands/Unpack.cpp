#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static bool read_line(std::istream& is, std::string& out) {
    out.clear();
    return static_cast<bool>(std::getline(is, out));
}

class Unpack : public ICommand {
public:
    std::string name() const override { return "unpack"; }
    std::string help() const override {
        return R"(unpack: extract a simple archive
Synopsis:
  unpack <archive> -C <vfs_path>
Notes:
  Extracts MiniArch v1 archives created by 'pack'. If -C is omitted,
  extracts into the current directory.
Examples:
  unpack demo.mar -C /restore
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) { ctx.out << "unpack: missing archive" << std::endl; return 2; }
        fs::path archive_vfs = to_vfs_path(ctx.args[1]);
        fs::path base_vfs = ctx.cwd;
        for (size_t i=2;i<ctx.args.size();++i){
            if (ctx.args[i] == "-C" && i+1 < ctx.args.size()) { base_vfs = to_vfs_path(ctx.args[++i]); }
        }

        fs::path archive_host, base_host;
        try { archive_host = ctx.vfs.resolveSecure(ctx.cwd, archive_vfs); }
        catch(const std::exception& e){ ctx.out << "unpack: " << e.what() << std::endl; return 1; }
        try { base_host = ctx.vfs.resolveSecure(ctx.cwd, base_vfs); }
        catch(const std::exception& e){ ctx.out << "unpack: " << e.what() << std::endl; return 1; }

        std::ifstream ifs(archive_host, std::ios::binary);
        if (!ifs) { ctx.out << "unpack: cannot open archive" << std::endl; return 1; }
        std::string line;
        if (!read_line(ifs, line) || line != "MINIARCH1") { ctx.out << "unpack: invalid archive header" << std::endl; return 1; }

        auto read_n = [&](size_t n) {
            std::string s; s.resize(n);
            ifs.read(&s[0], static_cast<std::streamsize>(n));
            return s;
        };

        while (true) {
            if (!read_line(ifs, line)) break; // EOF ok
            if (line.empty()) continue;
            if (line[0] == 'D') {
                // D <len>
                size_t sp = line.find(' ');
                size_t path_len = std::stoul(line.substr(sp+1));
                std::string rel = read_n(path_len);
                char nl; ifs.read(&nl, 1); // consume newline
                fs::path out = base_host / fs::path(rel);
                ctx.vfs.mkdir(out, true);
            } else if (line[0] == 'F') {
                // F <len> <size>
                size_t sp1 = line.find(' ');
                size_t sp2 = line.find(' ', sp1+1);
                size_t path_len = std::stoul(line.substr(sp1+1, sp2-(sp1+1)));
                size_t size = std::stoull(line.substr(sp2+1));
                std::string rel = read_n(path_len);
                char nl; ifs.read(&nl, 1);
                std::string data = read_n(size);
                fs::path out = base_host / fs::path(rel);
                ctx.vfs.writeFile(out, data, false);
            } else {
                ctx.out << "unpack: unknown entry" << std::endl; return 1;
            }
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_unpack(){ return std::make_unique<Unpack>(); } }

