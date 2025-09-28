#include "ExecDb.hpp"

#include "../vfs/IVfs.hpp"

#include <cctype>
#include <sstream>

namespace execdb {

namespace {

inline std::filesystem::path root_path(const char* p) {
    return std::filesystem::path(p);
}

}

std::unordered_set<std::string> load(IVfs& vfs) {
    std::unordered_set<std::string> result;
    try {
        auto execdb_host = vfs.resolveSecure(root_path("/"), root_path("/etc/execdb"));
        auto data = vfs.readFile(execdb_host);
        std::istringstream is(data);
        std::string line;
        while (std::getline(is, line)) {
            size_t start = 0;
            while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) ++start;
            size_t end = line.size();
            while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) --end;
            if (end > start) result.insert(line.substr(start, end - start));
        }
    } catch (const std::exception&) {
        // Missing database is treated as empty.
    }
    return result;
}

void save(IVfs& vfs, const std::unordered_set<std::string>& entries) {
    std::ostringstream os;
    for (const auto& entry : entries) {
        os << entry << '\n';
    }

    auto etc_host = vfs.resolveSecure(root_path("/"), root_path("/etc"));
    vfs.mkdir(etc_host, true);
    auto execdb_host = vfs.resolveSecure(root_path("/"), root_path("/etc/execdb"));
    vfs.writeFile(execdb_host, os.str(), false);
}

bool set(IVfs& vfs, const std::filesystem::path& host_path, bool enable) {
    auto db = load(vfs);
    auto key = host_path.generic_string();
    bool changed = false;
    if (enable) {
        changed = db.insert(key).second;
    } else {
        auto it = db.find(key);
        if (it != db.end()) {
            db.erase(it);
            changed = true;
        }
    }
    if (changed) {
        save(vfs, db);
    }
    return changed;
}

}
