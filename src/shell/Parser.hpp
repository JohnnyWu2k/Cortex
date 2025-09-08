#pragma once
#include <string>
#include <vector>

namespace Parser {
    // Split a line into args, handling simple quotes and escapes.
    std::vector<std::string> split(const std::string& line);
}

