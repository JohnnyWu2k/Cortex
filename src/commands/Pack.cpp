#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

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

        struct ResolvedSource {
            fs::path host;
            bool is_dir;
            std::string original;
        };

        std::vector<ResolvedSource> resolved;
        resolved.reserve(sources.size());
        for (auto& s : sources){
            fs::path vfs_p = to_vfs_path(s);
            fs::path host;
            try { host = ctx.vfs.resolveSecure(ctx.cwd, vfs_p); }
            catch(const std::exception& e){ ctx.out << "pack: " << e.what() << std::endl; return 1; }

            std::error_code ec;
            if (fs::is_directory(host, ec)){
                resolved.push_back({host, true, s});
            } else if (fs::is_regular_file(host, ec)){
                resolved.push_back({host, false, s});
            } else {
                ctx.out << "pack: no such file or directory: " << s << std::endl;
                return 1;
            }
        }

        if (resolved.empty()) {
            ctx.out << "pack: no source paths" << std::endl;
            return 2;
        }

        std::ofstream ofs(out_host, std::ios::binary | std::ios::trunc);
        if (!ofs) { ctx.out << "pack: cannot open output" << std::endl; return 1; }
        ofs << "MINIARCH1\n";

        size_t entries_emitted = 0;

        auto add_file = [&](const fs::path& host_root, const fs::path& host_file){
            std::error_code ec;
            auto rel = fs::relative(host_file, host_root, ec);
            if (ec) {
                throw std::runtime_error("pack: cannot compute relative path");
            }
            std::string rel_path = rel.generic_string();
            std::string data = ctx.vfs.readFile(host_file);
            ofs << "F " << rel_path.size() << ' ' << data.size() << "\n";
            ofs.write(rel_path.data(), (std::streamsize)rel_path.size());
            ofs << '\n';
            ofs.write(data.data(), (std::streamsize)data.size());
            ++entries_emitted;
        };

        auto add_dir_entry = [&](const fs::path& host_root, const fs::path& host_dir){
            std::error_code ec;
            auto rel = fs::relative(host_dir, host_root, ec);
            if (ec) {
                throw std::runtime_error("pack: cannot compute relative path");
            }
            std::string rel_path = rel.generic_string();
            ofs << "D " << rel_path.size() << "\n";
            ofs.write(rel_path.data(), (std::streamsize)rel_path.size());
            ofs << '\n';
            ++entries_emitted;
        };

        try {
            for (const auto& entry : resolved){
                std::error_code ec;
                if (entry.is_dir){
                    fs::path base = entry.host.parent_path();
                    add_dir_entry(base, entry.host);
                    fs::recursive_directory_iterator it(entry.host, fs::directory_options::skip_permission_denied, ec), end;
                    for (; it != end; ++it){
                        if (it->is_directory(ec)) add_dir_entry(base, it->path());
                        else if (it->is_regular_file(ec)) add_file(base, it->path());
                    }
                    if (ec) {
                        ctx.out << "pack: " << ec.message() << " (" << entry.original << ")" << std::endl;
                        std::error_code remove_ec;
                        fs::remove(out_host, remove_ec);
                        return 1;
                    }
                } else {
                    // For single file, root is its parent; rel = filename
                    add_file(entry.host.parent_path(), entry.host);
                }
            }
        } catch (const std::exception& e) {
            ctx.out << e.what() << std::endl;
            std::error_code remove_ec;
            fs::remove(out_host, remove_ec);
            return 1;
        }

        if (entries_emitted == 0) {
            ctx.out << "pack: no entries archived" << std::endl;
            std::error_code remove_ec;
            fs::remove(out_host, remove_ec);
            return 1;
        }
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_pack(){ return std::make_unique<Pack>(); } }
