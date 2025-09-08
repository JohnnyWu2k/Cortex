#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct DirEntry {
    std::string name;
    bool is_dir;
    uintmax_t size;
};

struct StatInfo {
    std::string name;
    bool is_dir;
    uintmax_t size;
    std::filesystem::file_time_type mtime;
};

class IVfs {
public:
    virtual ~IVfs() = default;

    virtual std::filesystem::path resolveSecure(const std::filesystem::path& cwd,
                                                const std::filesystem::path& input) const = 0;

    virtual std::vector<DirEntry> list(const std::filesystem::path& path) const = 0;
    virtual void touch(const std::filesystem::path& path) = 0;
    virtual void mkdir(const std::filesystem::path& path, bool recursive) = 0;
    virtual void remove(const std::filesystem::path& path, bool recursive) = 0;
    virtual void copy(const std::filesystem::path& src, const std::filesystem::path& dst, bool recursive) = 0;
    virtual void move(const std::filesystem::path& src, const std::filesystem::path& dst) = 0;
    virtual StatInfo stat(const std::filesystem::path& path) const = 0;
    virtual std::string readFile(const std::filesystem::path& path) const = 0;
    virtual void writeFile(const std::filesystem::path& path, const std::string& data, bool append) = 0;

    virtual const std::filesystem::path& root() const = 0;
};

