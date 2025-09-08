#include "Parser.hpp"

namespace Parser {

static void push_token(std::vector<std::string>& out, std::string& cur) {
    if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
    }
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> args;
    std::string cur;
    bool in_single = false, in_double = false, escape = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escape) { cur.push_back(c); escape = false; continue; }
        if (c == '\\') { escape = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (!in_single && !in_double) {
            if (c == ' ' || c == '\t') { push_token(args, cur); continue; }
            if (c == '|') { push_token(args, cur); args.emplace_back("|"); continue; }
        }
        cur.push_back(c);
    }
    push_token(args, cur);
    return args;
}

}
