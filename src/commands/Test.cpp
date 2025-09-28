#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#include "../vfs/IVfs.hpp"
#include "Helpers.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace {
    // Core evaluator for test/[ commands; returns 0 (true) or 1 (false), 2 for syntax error
    int eval_test(CommandContext& ctx, const std::vector<std::string>& args) {
        using namespace std;
        // Support forms:
        // [ EXPRESSION ]
        // test EXPRESSION
        // Minimal operators: -z s, -n s, s1 = s2, s1 != s2, -f path, -d path, -e path
        if (args.empty()) return 1; // empty expression -> false

        auto is_unary = [&](const string& op){ return op == "-z" || op == "-n" || op == "-f" || op == "-d" || op == "-e"; };

        // Normalize args (already expanded by shell)
        vector<string> a = args;
        if (a.size() == 1) {
            // Single operand: true if non-empty
            return a[0].empty() ? 1 : 0;
        }

        if (is_unary(a[0])) {
            if (a.size() != 2) return 2;
            const string& op = a[0];
            const string& operand = a[1];
            if (op == "-z") return operand.empty() ? 0 : 1;
            if (op == "-n") return operand.empty() ? 1 : 0;
            try {
                auto host = ctx.vfs.resolveSecure(ctx.cwd, to_vfs_path(operand));
                std::error_code ec;
                if (op == "-f") {
                    bool ok = std::filesystem::is_regular_file(host, ec);
                    return ok ? 0 : 1;
                }
                if (op == "-d") {
                    bool ok = std::filesystem::is_directory(host, ec);
                    return ok ? 0 : 1;
                }
                if (op == "-e") {
                    bool ok = std::filesystem::exists(host, ec);
                    return ok ? 0 : 1;
                }
            } catch (const std::exception&) {
                // resolve failed -> treat as not existing
                if (a[0] == "-e") return 1;
                if (a[0] == "-f") return 1;
                if (a[0] == "-d") return 1;
                return 2;
            }
            return 2;
        }

        // Binary string comparison: s1 = s2 | s1 != s2
        if (a.size() == 3 && (a[1] == "=" || a[1] == "!=")) {
            bool eq = (a[0] == a[2]);
            if (a[1] == "=") return eq ? 0 : 1;
            else return eq ? 1 : 0;
        }

        // Unsupported expression
        return 2;
    }

    class TestCmd : public ICommand {
    public:
        explicit TestCmd(std::string nm) : nm_(std::move(nm)) {}
        std::string name() const override { return nm_; }
        std::string help() const override {
            return R"(test/[ : evaluate expressions
Synopsis:
  test EXPRESSION
  [ EXPRESSION ]
Operators (MVP):
  -z s     true if s has length 0
  -n s     true if s has non-zero length
  s1 = s2  true if strings are equal
  s1 != s2 true if strings are not equal
  -f p     true if path is a regular file
  -d p     true if path is a directory
  -e p     true if path exists
Exit status: 0 true, 1 false, 2 syntax error)";
        }
        int execute(CommandContext& ctx) override {
            std::vector<std::string> expr;
            // For '[' command, last token must be ']' (allow trailing '];')
            if (nm_ == "[") {
                if (ctx.args.size() < 2) { ctx.out << "[: missing ']'" << std::endl; return 2; }
                // args: [ ... ]
                // strip first and last
                size_t n = ctx.args.size();
                std::string last = ctx.args.back();
                if (!last.empty() && last.back() == ';') last.pop_back();
                if (last != "]") { ctx.out << "[: missing ']'" << std::endl; return 2; }
                for (size_t i = 1; i + 1 < n; ++i) expr.push_back(ctx.args[i]);
            } else {
                // test ...
                for (size_t i = 1; i < ctx.args.size(); ++i) expr.push_back(ctx.args[i]);
            }
            int rc = eval_test(ctx, expr);
            return rc;
        }
    private:
        std::string nm_;
    };
}

namespace Builtins {
    std::unique_ptr<ICommand> make_test(){ return std::make_unique<TestCmd>("test"); }
    std::unique_ptr<ICommand> make_bracket(){ return std::make_unique<TestCmd>("["); }
}

