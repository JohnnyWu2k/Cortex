#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "../pkg/PackageManager.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

std::filesystem::path detect_repo_root() {
    std::vector<std::filesystem::path> candidates;

    if (const char* env = std::getenv("CORTEX_PKG_ROOT")) {
        if (*env) candidates.emplace_back(env);
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        candidates.emplace_back(cwd / "packages");
        candidates.emplace_back(cwd.parent_path() / "packages");
    }

    for (const auto& dir : candidates) {
        if (!dir.empty()) {
            auto index = dir / "index.ini";
            if (std::filesystem::exists(index)) {
                return dir;
            }
        }
    }

    if (!candidates.empty()) return candidates.front();
    return cwd / "packages";
}

void print_usage(std::ostream& out) {
    out << "pkg: package manager for Cortex scripts" << std::endl;
    out << "Usage:" << std::endl;
    out << "  pkg list" << std::endl;
    out << "  pkg info <name>" << std::endl;
    out << "  pkg install <name>" << std::endl;
    out << "  pkg remove <name>" << std::endl;
    out << "  pkg installed" << std::endl;
}

void render_list(const std::vector<pkg::Package>& packages, const pkg::Manager& manager, std::ostream& out) {
    if (packages.empty()) {
        out << "pkg: repository is empty" << std::endl;
        return;
    }
    size_t max_name = 4;
    size_t max_version = 7;
    for (const auto& pkg : packages) {
        max_name = std::max(max_name, pkg.name.size());
        max_version = std::max(max_version, pkg.version.size());
    }
    out << std::left << std::setw(static_cast<int>(max_name)) << "Name" << "  "
        << std::setw(static_cast<int>(max_version)) << "Version" << "  Description" << std::endl;
    for (const auto& pkg : packages) {
        bool installed = manager.is_installed(pkg.name);
        out << std::left << std::setw(static_cast<int>(max_name)) << pkg.name << "  "
            << std::setw(static_cast<int>(max_version)) << pkg.version << "  "
            << pkg.description;
        if (installed) out << "  [installed]";
        out << std::endl;
    }
}

void render_installed(const pkg::Manager& manager, std::ostream& out) {
    auto entries = manager.installed();
    if (entries.empty()) {
        out << "pkg: no packages installed" << std::endl;
        return;
    }
    size_t max_name = 4;
    size_t max_version = 7;
    for (const auto& pkg : entries) {
        max_name = std::max(max_name, pkg.name.size());
        max_version = std::max(max_version, pkg.version.size());
    }
    out << std::left << std::setw(static_cast<int>(max_name)) << "Name" << "  "
        << std::setw(static_cast<int>(max_version)) << "Version" << "  Path" << std::endl;
    for (const auto& pkg : entries) {
        out << std::left << std::setw(static_cast<int>(max_name)) << pkg.name << "  "
            << std::setw(static_cast<int>(max_version)) << pkg.version << "  "
            << pkg.install_path << std::endl;
    }
}

}

class Pkg : public ICommand {
public:
    std::string name() const override { return "pkg"; }
    std::string help() const override {
        return R"(pkg: manage Cortex package scripts
Synopsis:
  pkg list
  pkg info <name>
  pkg install <name>
  pkg remove <name>
  pkg installed
Environment:
  CORTEX_PKG_ROOT  Override package repository root directory
)";
    }
    int execute(CommandContext& ctx) override {
        if (ctx.args.size() < 2) {
            print_usage(ctx.out);
            return 2;
        }

        auto repo_root = detect_repo_root();
        pkg::Manager manager(ctx.vfs, repo_root);
        if (!manager.load()) {
            ctx.out << "pkg: " << manager.error() << std::endl;
            ctx.out << "提示: 設定 CORTEX_PKG_ROOT 指向套件倉庫目錄" << std::endl;
            return 1;
        }

        std::string sub = ctx.args[1];
        if (sub == "list") {
            render_list(manager.packages(), manager, ctx.out);
            return 0;
        }
        if (sub == "installed") {
            render_installed(manager, ctx.out);
            return 0;
        }
        if (sub == "info") {
            if (ctx.args.size() < 3) {
                ctx.out << "pkg: info requires a package name" << std::endl;
                return 2;
            }
            auto pkg = manager.find(ctx.args[2]);
            if (!pkg) {
                ctx.out << "pkg: unknown package '" << ctx.args[2] << "'" << std::endl;
                return 1;
            }
            ctx.out << "Name: " << pkg->name << std::endl;
            ctx.out << "Version: " << pkg->version << std::endl;
            ctx.out << "Type: " << pkg->type << std::endl;
            ctx.out << "Description: " << pkg->description << std::endl;
            ctx.out << "Install Path: " << pkg->install_path << std::endl;
            ctx.out << "Source: " << (manager.is_installed(pkg->name) ? "(installed) " : "")
                    << repo_root / pkg->source << std::endl;
            if (auto inst = manager.installed_info(pkg->name)) {
                ctx.out << "Status: installed (" << inst->version << ')' << std::endl;
            } else {
                ctx.out << "Status: not installed" << std::endl;
            }
            return 0;
        }
        if (sub == "install") {
            if (ctx.args.size() < 3) {
                ctx.out << "pkg: install requires a package name" << std::endl;
                return 2;
            }
            return manager.install(ctx.args[2], ctx.out, ctx.out) ? 0 : 1;
        }
        if (sub == "remove") {
            if (ctx.args.size() < 3) {
                ctx.out << "pkg: remove requires a package name" << std::endl;
                return 2;
            }
            return manager.remove(ctx.args[2], ctx.out, ctx.out) ? 0 : 1;
        }

        ctx.out << "pkg: unknown subcommand '" << sub << "'" << std::endl;
        print_usage(ctx.out);
        return 2;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_pkg(){ return std::make_unique<Pkg>(); } }
