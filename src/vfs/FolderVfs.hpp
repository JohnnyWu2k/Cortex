#pragma once
#include "IVfs.hpp"
#include <filesystem>

class FolderVfs : public IVfs {
public:
    explicit FolderVfs(std::filesystem::path root);

    std::filesystem::path resolveSecure(const std::filesystem::path& cwd,
                                        const std::filesystem::path& input) const override;

    std::vector<DirEntry> list(const std::filesystem::path& path) const override;
    void touch(const std::filesystem::path& path) override;
    void mkdir(const std::filesystem::path& path, bool recursive) override;
    void remove(const std::filesystem::path& path, bool recursive) override;
    void copy(const std::filesystem::path& src, const std::filesystem::path& dst, bool recursive) override;
    void move(const std::filesystem::path& src, const std::filesystem::path& dst) override;
    StatInfo stat(const std::filesystem::path& path) const override;
    std::string readFile(const std::filesystem::path& path) const override;
    void writeFile(const std::filesystem::path& path, const std::string& data, bool append) override;

    const std::filesystem::path& root() const override { return root_; }

private:
    std::filesystem::path root_;
};

