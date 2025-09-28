#include "PackageManager.hpp"

#include "../util/ExecDb.hpp"
#include "../vfs/IVfs.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace pkg {

namespace {

std::string trim(const std::string& in) {
    size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) ++start;
    size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) --end;
    return in.substr(start, end - start);
}

bool looks_like_absolute_vfs_path(const std::string& path) {
    return !path.empty() && path.front() == '/';
}

}

Repository::Repository(std::filesystem::path root)
    : root_(std::move(root)) {}

bool Repository::load() {
    packages_.clear();
    error_.clear();

    auto index_path = root_ / "index.ini";
    std::ifstream ifs(index_path);
    if (!ifs) {
        error_ = "repository index not found: " + index_path.string();
        return false;
    }

    auto push_package = [&](Package&& pkg) {
        if (pkg.name.empty()) {
            error_ = "package entry missing name";
            return false;
        }
        if (pkg.source.empty()) {
            error_ = "package '" + pkg.name + "' missing source";
            return false;
        }
        if (!looks_like_absolute_vfs_path(pkg.install_path)) {
            error_ = "package '" + pkg.name + "' install path must be absolute";
            return false;
        }
        if (pkg.version.empty()) pkg.version = "0.0.0";
        if (pkg.type.empty()) pkg.type = "script";
        packages_.push_back(std::move(pkg));
        return true;
    };

    std::string line;
    Package current;
    bool in_pkg = false;

    while (std::getline(ifs, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            if (in_pkg) {
                if (!push_package(std::move(current))) return false;
                current = Package{};
            }
            current.name = trim(trimmed.substr(1, trimmed.size() - 2));
            in_pkg = true;
            continue;
        }
        auto eq = trimmed.find('=');
        if (eq == std::string::npos) {
            error_ = "invalid line in index: " + trimmed;
            return false;
        }
        auto key = trim(trimmed.substr(0, eq));
        auto value = trim(trimmed.substr(eq + 1));
        if (key == "version") current.version = value;
        else if (key == "description") current.description = value;
        else if (key == "source") current.source = value;
        else if (key == "install") current.install_path = value;
        else if (key == "type") current.type = value;
    }

    if (in_pkg) {
        if (!push_package(std::move(current))) return false;
    }

    return true;
}

const Package* Repository::find(const std::string& name) const {
    auto it = std::find_if(packages_.begin(), packages_.end(), [&](const Package& pkg){ return pkg.name == name; });
    if (it == packages_.end()) return nullptr;
    return &(*it);
}

InstalledDb::InstalledDb(IVfs& vfs)
    : vfs_(vfs) {}

bool InstalledDb::load() {
    if (loaded_) return true;
    entries_.clear();

    try {
        auto db_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/var/lib/pkg/installed.db"));
        auto data = vfs_.readFile(db_host);
        std::istringstream is(data);
        std::string line;
        while (std::getline(is, line)) {
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            auto first = trimmed.find('|');
            auto second = trimmed.find('|', first == std::string::npos ? std::string::npos : first + 1);
            if (first == std::string::npos || second == std::string::npos) continue;
            InstalledPackage pkg;
            pkg.name = trimmed.substr(0, first);
            pkg.version = trimmed.substr(first + 1, second - first - 1);
            pkg.install_path = trimmed.substr(second + 1);
            entries_.push_back(std::move(pkg));
        }
    } catch (const std::exception&) {
        // Treat missing file as empty database.
    }

    loaded_ = true;
    dirty_ = false;
    return true;
}

bool InstalledDb::save() {
    if (!dirty_) return true;

    auto dir_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/var/lib/pkg"));
    vfs_.mkdir(dir_host, true);
    auto db_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path("/var/lib/pkg/installed.db"));

    std::ostringstream os;
    for (const auto& pkg : entries_) {
        os << pkg.name << '|' << pkg.version << '|' << pkg.install_path << '\n';
    }
    vfs_.writeFile(db_host, os.str(), false);
    dirty_ = false;
    return true;
}

const InstalledPackage* InstalledDb::find(const std::string& name) const {
    auto it = std::find_if(entries_.begin(), entries_.end(), [&](const InstalledPackage& pkg){ return pkg.name == name; });
    if (it == entries_.end()) return nullptr;
    return &(*it);
}

void InstalledDb::set(InstalledPackage entry) {
    auto it = std::find_if(entries_.begin(), entries_.end(), [&](const InstalledPackage& pkg){ return pkg.name == entry.name; });
    if (it == entries_.end()) {
        entries_.push_back(std::move(entry));
    } else {
        *it = std::move(entry);
    }
    dirty_ = true;
}

void InstalledDb::remove(const std::string& name) {
    auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const InstalledPackage& pkg){ return pkg.name == name; });
    if (it != entries_.end()) {
        entries_.erase(it, entries_.end());
        dirty_ = true;
    }
}

Manager::Manager(IVfs& vfs, std::filesystem::path repo_root)
    : vfs_(vfs)
    , repo_root_(std::move(repo_root))
    , repo_(repo_root_)
    , installed_db_(vfs_) {}

bool Manager::load() {
    if (!ensure_repository_loaded()) return false;
    if (!ensure_installed_loaded()) return false;
    return true;
}

const std::vector<Package>& Manager::packages() const {
    ensure_repository_loaded();
    return repo_.packages();
}

const Package* Manager::find(const std::string& name) const {
    if (!ensure_repository_loaded()) return nullptr;
    return repo_.find(name);
}

bool Manager::is_installed(const std::string& name) const {
    return installed_info(name) != nullptr;
}

const InstalledPackage* Manager::installed_info(const std::string& name) const {
    if (!ensure_installed_loaded()) return nullptr;
    return installed_db_.find(name);
}

const std::vector<InstalledPackage>& Manager::installed() const {
    ensure_installed_loaded();
    return installed_db_.entries();
}

bool Manager::install(const std::string& name, std::ostream& out, std::ostream& err) {
    if (!ensure_repository_loaded() || !ensure_installed_loaded()) return false;
    auto pkg = repo_.find(name);
    if (!pkg) {
        err << "pkg: unknown package '" << name << "'" << std::endl;
        return false;
    }
    if (installed_db_.find(name)) {
        err << "pkg: '" << name << "' is already installed" << std::endl;
        return false;
    }

    bool ok = false;
    if (pkg->type == "script") {
        ok = install_script(*pkg, out, err);
    } else {
        err << "pkg: unsupported package type '" << pkg->type << "'" << std::endl;
        return false;
    }
    if (!ok) return false;

    InstalledPackage entry;
    entry.name = pkg->name;
    entry.version = pkg->version;
    entry.install_path = pkg->install_path;
    installed_db_.set(std::move(entry));
    if (!installed_db_.save()) {
        err << "pkg: failed to update installed database" << std::endl;
        return false;
    }
    out << "pkg: installed '" << pkg->name << "' (" << pkg->version << ")" << std::endl;
    return true;
}

bool Manager::remove(const std::string& name, std::ostream& out, std::ostream& err) {
    if (!ensure_repository_loaded() || !ensure_installed_loaded()) return false;
    auto info = installed_db_.find(name);
    if (!info) {
        err << "pkg: '" << name << "' is not installed" << std::endl;
        return false;
    }

    bool ok = remove_script(*info, out, err);
    if (!ok) return false;

    installed_db_.remove(name);
    if (!installed_db_.save()) {
        err << "pkg: failed to update installed database" << std::endl;
        return false;
    }
    out << "pkg: removed '" << name << "'" << std::endl;
    return true;
}

bool Manager::ensure_repository_loaded() const {
    if (repo_loaded_) return error_.empty();
    repo_loaded_ = true;
    if (!repo_.load()) {
        error_ = repo_.error();
        return false;
    }
    return true;
}

bool Manager::ensure_installed_loaded() const {
    if (installed_loaded_) return true;
    installed_loaded_ = true;
    if (!installed_db_.load()) {
        error_ = "failed to load installed package database";
        return false;
    }
    return true;
}

std::filesystem::path Manager::source_path_for(const Package& pkg) const {
    return repo_root_ / pkg.source;
}

bool Manager::install_script(const Package& pkg, std::ostream& out, std::ostream& err) {
    (void)out;
    auto src = source_path_for(pkg);
    std::ifstream ifs(src, std::ios::binary);
    if (!ifs) {
        err << "pkg: cannot open package payload: " << src << std::endl;
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    auto target_host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path(pkg.install_path));
    auto parent = target_host.parent_path();
    if (!parent.empty()) {
        vfs_.mkdir(parent, true);
    }
    vfs_.writeFile(target_host, data, false);
    execdb::set(vfs_, target_host, true);
    return true;
}

bool Manager::remove_script(const InstalledPackage& pkg, std::ostream& /*out*/, std::ostream& err) {
    auto host = vfs_.resolveSecure(std::filesystem::path("/"), std::filesystem::path(pkg.install_path));
    try {
        vfs_.remove(host, false);
    } catch (const std::exception& e) {
        err << "pkg: failed to remove file: " << e.what() << std::endl;
        return false;
    }
    execdb::set(vfs_, host, false);
    return true;
}

}
