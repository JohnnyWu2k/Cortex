#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

static void write_u64(std::ostream& os, uint64_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

class Pack : public ICommand {
public:
    std::string name() const override { return "pack"; }
    std::string help() const override {
        return R"(pack: create a simple archive from files/dirs
Synopsis:
  pack <source_path...> -o <output_archive>
Notes:
  Creates a simple uncompressed archive (MiniArch v1). Paths are stored
  relative to each given source; directories are included to preserve layout.
Examples:
  pack /projects/demo -o /backup/demo.mar
  pack a.txt b.txt -o files.mar
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 3) { ctx.out << "pack: missing arguments" << std::endl; return 2; }
        std::vector<std::string> sources;
        fs::path out_vfs;
        for (size_t i=1;i<ctx.args.size();++i){
            if (ctx.args[i] == "-o" && i+1 < ctx.args.size()) { out_vfs = to_vfs_path(ctx.args[++i]); continue; }
            sources.push_back(ctx.args[i]);
        }
        if (out_vfs.empty()) { ctx.out << "pack: missing -o <output_archive>" << std::endl; return 2; }
        if (sources.empty()) { ctx.out << "pack: no source paths" << std::endl; return 2; }

        fs::path out_host;
        try { out_host = ctx.vfs.resolveSecure(ctx.cwd, out_vfs); }
        catch (const std::exception& e) { ctx.out << "pack: " << e.what() << std::endl; return 1; }

        std::ofstream ofs(out_host, std::ios::binary | std::ios::trunc);
        if (!ofs) { ctx.out << "pack: cannot open output" << std::endl; return 1; }
        ofs << "MINIARCH1\n";

        auto add_file = [&](const fs::path& host_root, const fs::path& host_file){
            std::error_code ec;
            auto rel = fs::relative(host_file, host_root, ec);
            std::string rel_path = rel.generic_string();
            std::string data = ctx.vfs.readFile(host_file);
            ofs << "F " << rel_path.size() << ' ' << data.size() << "\n";
            ofs.write(rel_path.data(), (std::streamsize)rel_path.size());
            ofs << '\n';
            ofs.write(data.data(), (std::streamsize)data.size());
        };

        auto add_dir_entry = [&](const fs::path& host_root, const fs::path& host_dir){
            std::error_code ec;
            auto rel = fs::relative(host_dir, host_root, ec);
            std::string rel_path = rel.generic_string();
            ofs << "D " << rel_path.size() << "\n";
            ofs.write(rel_path.data(), (std::streamsize)rel_path.size());
            ofs << '\n';
        };

        for (auto& s : sources){
            fs::path vfs_p = to_vfs_path(s);
            fs::path host;
            try { host = ctx.vfs.resolveSecure(ctx.cwd, vfs_p); }
            catch(const std::exception& e){ ctx.out << "pack: " << e.what() << std::endl; return 1; }

            std::error_code ec;
            if (fs::is_directory(host, ec)){
                add_dir_entry(host, host); // root dir entry as "." (empty rel)
                for (fs::recursive_directory_iterator it(host, fs::directory_options::skip_permission_denied, ec), end; it!=end; ++it){
                    if (it->is_directory(ec)) add_dir_entry(host, it->path());
                    else if (it->is_regular_file(ec)) add_file(host, it->path());
                }
            } else if (fs::is_regular_file(host, ec)){
                // For single file, root is its parent; rel = filename
                add_file(host.parent_path(), host);
            } else {
                ctx.out << "pack: skip unknown path: " << s << std::endl;
            }
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_pack(){ return std::make_unique<Pack>(); } }

