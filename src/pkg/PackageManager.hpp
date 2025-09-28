#pragma once

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

class IVfs;

namespace pkg {

struct Package {
    std::string name;
    std::string version;
    std::string description;
    std::string type;
    std::string source;
    std::string install_path;
};

struct InstalledPackage {
    std::string name;
    std::string version;
    std::string install_path;
};

class Repository {
public:
    explicit Repository(std::filesystem::path root);

    bool load();
    const std::vector<Package>& packages() const { return packages_; }
    const Package* find(const std::string& name) const;
    const std::string& error() const { return error_; }
    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
    std::vector<Package> packages_;
    std::string error_;
};

class InstalledDb {
public:
    explicit InstalledDb(IVfs& vfs);

    bool load();
    bool save();

    const InstalledPackage* find(const std::string& name) const;
    void set(InstalledPackage entry);
    void remove(const std::string& name);
    const std::vector<InstalledPackage>& entries() const { return entries_; }

private:
    IVfs& vfs_;
    std::vector<InstalledPackage> entries_;
    bool dirty_ = false;
    bool loaded_ = false;
};

class Manager {
public:
    Manager(IVfs& vfs, std::filesystem::path repo_root);

    bool load();
    const std::string& error() const { return error_; }

    const std::vector<Package>& packages() const;
    const Package* find(const std::string& name) const;

    bool is_installed(const std::string& name) const;
    const InstalledPackage* installed_info(const std::string& name) const;
    const std::vector<InstalledPackage>& installed() const;

    bool install(const std::string& name, std::ostream& out, std::ostream& err);
    bool remove(const std::string& name, std::ostream& out, std::ostream& err);

private:
    bool ensure_repository_loaded() const;
    bool ensure_installed_loaded() const;
    std::filesystem::path source_path_for(const Package& pkg) const;
    bool install_script(const Package& pkg, std::ostream& out, std::ostream& err);
    bool remove_script(const InstalledPackage& pkg, std::ostream& out, std::ostream& err);

    IVfs& vfs_;
    std::filesystem::path repo_root_;
    mutable Repository repo_;
    mutable InstalledDb installed_db_;
    mutable bool repo_loaded_ = false;
    mutable bool installed_loaded_ = false;
    mutable std::string error_;
};

}

