#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "runtime/cpu/cpu_context.h"
#include "runtime/memory/guest_memory.h"
#include "runtime/result.h"
#include "runtime/types.h"

namespace chronos {

enum class CpuHaltReason {
    Halted,
    Svc,
    Breakpoint,
    MemoryFault,
    IllegalInstruction,
};

constexpr std::string_view CpuHaltReasonToString(CpuHaltReason reason) noexcept {
    switch (reason) {
    case CpuHaltReason::Halted:
        return "Halted";
    case CpuHaltReason::Svc:
        return "Svc";
    case CpuHaltReason::Breakpoint:
        return "Breakpoint";
    case CpuHaltReason::MemoryFault:
        return "MemoryFault";
    case CpuHaltReason::IllegalInstruction:
        return "IllegalInstruction";
    }
    return "Unknown";
}

using SvcHandler = std::function<void(uint32_t swi)>;

class GuestCpu {
public:
    virtual ~GuestCpu() = default;

    [[nodiscard]] virtual Arch arch() const noexcept = 0;

    virtual void LoadContext(const CpuContext32& ctx) = 0;
    virtual void LoadContext(const CpuContext64& ctx) = 0;
    virtual void SaveContext(CpuContext32& ctx) const = 0;
    virtual void SaveContext(CpuContext64& ctx) const = 0;

    virtual Result<CpuHaltReason> Run() = 0;
    virtual void Step() = 0;
    virtual void Stop() = 0;

    virtual void SetSvcHandler(SvcHandler handler) = 0;
};

std::unique_ptr<GuestCpu> CreateGuestCpu(Arch arch, GuestMemory& memory);

} // namespace chronos
