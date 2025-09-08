#pragma once
#include <string>
#include <filesystem>

inline std::filesystem::path to_vfs_path(const std::string& s) {
    std::filesystem::path p{s};
    if (p.empty()) return std::filesystem::path(".");
    return p;
}

