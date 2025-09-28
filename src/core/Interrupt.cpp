#include "Interrupt.hpp"

namespace {
    std::atomic<bool> g_interrupted{false};
}

namespace Interrupt {
    bool check() { return g_interrupted.load(std::memory_order_relaxed); }
    void set() { g_interrupted.store(true, std::memory_order_relaxed); }
    void clear() { g_interrupted.store(false, std::memory_order_relaxed); }
}

