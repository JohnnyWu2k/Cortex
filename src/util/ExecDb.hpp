#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

class IVfs;

namespace execdb {

// Load the execute-permission database from /etc/execdb inside the VFS.
std::unordered_set<std::string> load(IVfs& vfs);

// Persist the execute-permission database back to /etc/execdb.
void save(IVfs& vfs, const std::unordered_set<std::string>& entries);

// Enable or disable execute permission for the given host path.
// Returns true if the value changed.
bool set(IVfs& vfs, const std::filesystem::path& host_path, bool enable);

}

