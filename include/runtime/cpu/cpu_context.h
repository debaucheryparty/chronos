#pragma once

#include <array>
#include <cstdint>

#include "runtime/types.h"

namespace chronos {

struct VectorRegister {
    uint64_t low = 0;
    uint64_t high = 0;

    bool operator==(const VectorRegister&) const = default;
};

struct CpuContext32 {
    std::array<uint32_t, 16> r{};
    uint32_t cpsr = 0;
    uint32_t fpscr = 0;
    std::array<uint64_t, 32> d{};

    [[nodiscard]] uint32_t sp() const noexcept { return r[13]; }
    void set_sp(uint32_t val) noexcept { r[13] = val; }

    [[nodiscard]] uint32_t lr() const noexcept { return r[14]; }
    void set_lr(uint32_t val) noexcept { r[14] = val; }

    [[nodiscard]] uint32_t pc() const noexcept { return r[15]; }
    void set_pc(uint32_t val) noexcept { r[15] = val; }

    bool operator==(const CpuContext32&) const = default;
};

struct CpuContext64 {
    std::array<uint64_t, 31> x{};
    uint64_t sp = 0;
    uint64_t pc = 0;
    uint32_t pstate = 0;
    uint32_t fpcr = 0;
    uint32_t fpsr = 0;
    std::array<VectorRegister, 32> v{};

    [[nodiscard]] uint64_t lr() const noexcept { return x[30]; }
    void set_lr(uint64_t val) noexcept { x[30] = val; }

    bool operator==(const CpuContext64&) const = default;
};

} // namespace chronos
