#include "FolderVfs.hpp"
#include <fstream>
#include <system_error>

using namespace std::filesystem;

static path canonical_or_weak(const path& p) {
    std::error_code ec;
    auto r = weakly_canonical(p, ec);
    if (ec) return p.lexically_normal();
    return r;
}

FolderVfs::FolderVfs(std::filesystem::path root) : root_(std::move(root)) {
    std::error_code ec;
    create_directories(root_, ec);
}

std::filesystem::path FolderVfs::resolveSecure(const std::filesystem::path& cwd,
                                               const std::filesystem::path& input) const {
    path base = input.is_absolute() ? path("/") : cwd;
    path vfs_path = canonical_or_weak(base / input).lexically_normal();
    // map to host path
    path host = canonical_or_weak(root_ / vfs_path.relative_path());
    // ensure within root
    auto root_can = canonical_or_weak(root_);
    auto host_str = host.native();
    auto root_str = root_can.native();
    if (host_str.size() < root_str.size() || host_str.substr(0, root_str.size()) != root_str) {
        throw std::runtime_error("security: path escapes VFS root");
    }
    return host;
}

std::vector<DirEntry> FolderVfs::list(const std::filesystem::path& path) const {
    std::vector<DirEntry> out;
    for (auto& de : directory_iterator(path)) {
        DirEntry e;
        e.name = de.path().filename().string();
        std::error_code ec;
        e.is_dir = is_directory(de.path(), ec);
        e.size = e.is_dir ? 0 : file_size(de.path(), ec);
        out.push_back(std::move(e));
    }
    return out;
}

void FolderVfs::touch(const std::filesystem::path& path) {
    std::error_code ec;
    if (!exists(path)) {
        create_directories(path.parent_path(), ec);
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) throw std::runtime_error("touch: cannot create file");
    } else {
        // Update mtime by appending zero bytes
        std::ofstream ofs(path, std::ios::binary | std::ios::app);
        if (!ofs) throw std::runtime_error("touch: cannot update file time");
    }
}

void FolderVfs::mkdir(const std::filesystem::path& path, bool recursive) {
    std::error_code ec;
    if (recursive) create_directories(path, ec); else create_directory(path, ec);
    if (ec) throw std::runtime_error("mkdir: " + ec.message());
}

void FolderVfs::remove(const std::filesystem::path& path, bool recursive) {
    std::error_code ec;
    if (recursive) {
        remove_all(path, ec);
    } else {
        std::filesystem::remove(path, ec);
    }
    if (ec) throw std::runtime_error("rm: " + ec.message());
}

void FolderVfs::copy(const std::filesystem::path& src, const std::filesystem::path& dst, bool recursive) {
    std::error_code ec;
    copy_options opts = copy_options::none;
    if (recursive) opts = copy_options::recursive | copy_options::overwrite_existing;
    else opts = copy_options::overwrite_existing;
    std::filesystem::copy(src, dst, opts, ec);
    if (ec) throw std::runtime_error("cp: " + ec.message());
}

void FolderVfs::move(const std::filesystem::path& src, const std::filesystem::path& dst) {
    std::error_code ec;
    std::filesystem::rename(src, dst, ec);
    if (ec) throw std::runtime_error("mv: " + ec.message());
}

StatInfo FolderVfs::stat(const std::filesystem::path& path) const {
    std::error_code ec;
    StatInfo s;
    s.name = path.filename().string();
    s.is_dir = is_directory(path, ec);
    s.size = s.is_dir ? 0 : file_size(path, ec);
    s.mtime = last_write_time(path, ec);
    if (ec) throw std::runtime_error("stat: " + ec.message());
    return s;
}

std::string FolderVfs::readFile(const std::filesystem::path& path) const {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("cat: cannot open file");
    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return data;
}

void FolderVfs::writeFile(const std::filesystem::path& path, const std::string& data, bool append) {
    std::error_code ec;
    create_directories(path.parent_path(), ec);
    std::ofstream ofs(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    if (!ofs) throw std::runtime_error("write: cannot open file");
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
}
