#include "../shell/ICommand.hpp"
#include "../shell/CommandContext.hpp"
#ifdef _WIN32
#  include <windows.h>
#endif

class Clear : public ICommand {
public:
    std::string name() const override { return "clear"; }
    std::string help() const override {
        return R"(clear: clear the screen
Synopsis:
  clear
Notes:
  Clears the console display. Behavior may vary by terminal.)";
    }
    int execute(CommandContext& ctx) override {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
                DWORD cellCount = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
                COORD home = {0, 0};
                DWORD count;
                FillConsoleOutputCharacter(hOut, ' ', cellCount, home, &count);
                FillConsoleOutputAttribute(hOut, csbi.wAttributes, cellCount, home, &count);
                SetConsoleCursorPosition(hOut, home);
                return 0;
            }
        }
        // If not a console, do nothing (avoid printing control chars)
        return 0;
#else
        ctx.out << "\x1b[2J\x1b[H";
        return 0;
#endif
        return 0;
    }
};

namespace Builtins { std::unique_ptr<ICommand> make_clear(){ return std::make_unique<Clear>(); } }
