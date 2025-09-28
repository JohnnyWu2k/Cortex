#pragma once
#include <atomic>

namespace Interrupt {
    // Returns true if an interrupt (e.g., Ctrl+C) was requested
    bool check();
    // Set interrupt flag (signal-safe usage expected from handlers)
    void set();
    // Clear interrupt flag
    void clear();
}

