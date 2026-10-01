#include "runtime/cpu/guest_cpu.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "runtime/logging.h"

namespace chronos {

namespace {

class InterpreterCpu final : public GuestCpu {
public:
    InterpreterCpu(Arch arch, GuestMemory& memory)
        : arch_(arch), memory_(memory) {}

    [[nodiscard]] Arch arch() const noexcept override {
        return arch_;
    }

    void LoadContext(const CpuContext32& ctx) override {
        ctx32_ = ctx;
    }

    void LoadContext(const CpuContext64& ctx) override {
        ctx64_ = ctx;
    }

    void SaveContext(CpuContext32& ctx) const override {
        ctx = ctx32_;
    }

    void SaveContext(CpuContext64& ctx) const override {
        ctx = ctx64_;
    }

    Result<CpuHaltReason> Run() override {
        running_ = true;
        while (running_) {
            if (arch_ == Arch::Arm64) {
                auto reason = StepAArch64();
                if (reason != CpuHaltReason::Halted || !running_) {
                    return reason;
                }
            } else {
                auto reason = StepArm32();
                if (reason != CpuHaltReason::Halted || !running_) {
                    return reason;
                }
            }
        }
        return CpuHaltReason::Halted;
    }

    void Step() override {
        if (arch_ == Arch::Arm64) {
            StepAArch64();
        } else {
            StepArm32();
        }
    }

    void Stop() override {
        running_ = false;
    }

    void SetSvcHandler(SvcHandler handler) override {
        svc_handler_ = std::move(handler);
    }

private:
    CpuHaltReason StepAArch64() {
        uint32_t insn = 0;
        auto read_res = memory_.Read(ctx64_.pc, &insn, sizeof(insn));
        if (!read_res) {
            return CpuHaltReason::MemoryFault;
        }

        const uint64_t current_pc = ctx64_.pc;
        ctx64_.pc += 4;

        // RET (0xd65f03c0) or RET Xn
        if ((insn & 0xfffffc1f) == 0xd65f0000) {
            const size_t rn = (insn >> 5) & 0x1f;
            const uint64_t target = (rn == 31) ? ctx64_.sp : ctx64_.x[rn];
            ctx64_.pc = target;
            running_ = false;
            return CpuHaltReason::Halted;
        }

        // SVC #imm16 (0xd4000001 with imm in bits 5..20)
        if ((insn & 0xffe0001f) == 0xd4000001) {
            const uint32_t imm16 = (insn >> 5) & 0xffff;
            if (svc_handler_) {
                svc_handler_(imm16);
            }
            return CpuHaltReason::Svc;
        }

        // BRK #imm16 (0xd4200000)
        if ((insn & 0xffe0001f) == 0xd4200000) {
            running_ = false;
            return CpuHaltReason::Breakpoint;
        }

        // MOVZ Xd, #imm16, LSL #shift (0xd2800000)
        if ((insn & 0x7f800000) == 0x52800000) {
            const bool is_64 = (insn >> 31) & 1;
            const size_t rd = insn & 0x1f;
            const uint64_t imm16 = (insn >> 5) & 0xffff;
            const unsigned hw = (insn >> 21) & 3;
            const uint64_t val = imm16 << (hw * 16);

            if (rd != 31) {
                ctx64_.x[rd] = is_64 ? val : static_cast<uint32_t>(val);
            }
            return CpuHaltReason::Halted;
        }

        // ADD / SUB (shifted register, 64-bit: 0x8b000000 / 0xcb000000)
        if ((insn & 0x5f200000) == 0x0b000000) {
            const bool is_64 = (insn >> 31) & 1;
            const bool is_sub = (insn >> 30) & 1;
            const size_t rm = (insn >> 16) & 0x1f;
            const size_t rn = (insn >> 5) & 0x1f;
            const size_t rd = insn & 0x1f;

            const uint64_t val_n = (rn == 31) ? 0 : ctx64_.x[rn];
            const uint64_t val_m = (rm == 31) ? 0 : ctx64_.x[rm];
            const uint64_t res = is_sub ? (val_n - val_m) : (val_n + val_m);

            if (rd != 31) {
                ctx64_.x[rd] = is_64 ? res : static_cast<uint32_t>(res);
            }
            return CpuHaltReason::Halted;
        }

        // Fallback for unsupported test instruction
        LogWarn("Cpu", "Unhandled AArch64 instruction 0x{:08x} at pc=0x{:016x}", insn, current_pc);
        return CpuHaltReason::IllegalInstruction;
    }

    CpuHaltReason StepArm32() {
        uint32_t insn = 0;
        auto read_res = memory_.Read(ctx32_.pc(), &insn, sizeof(insn));
        if (!read_res) {
            return CpuHaltReason::MemoryFault;
        }

        const uint32_t current_pc = ctx32_.pc();
        ctx32_.set_pc(current_pc + 4);

        // BX Rm (0xe12fff10)
        if ((insn & 0x0ffffff0) == 0x012fff10) {
            const size_t rm = insn & 0xf;
            const uint32_t target = ctx32_.r[rm] & ~1u; // clear thumb bit
            ctx32_.set_pc(target);
            running_ = false;
            return CpuHaltReason::Halted;
        }

        // SVC #imm24 (0xef000000)
        if ((insn & 0x0f000000) == 0x0f000000) {
            const uint32_t imm24 = insn & 0x00ffffff;
            if (svc_handler_) {
                svc_handler_(imm24);
            }
            return CpuHaltReason::Svc;
        }

        // MOV Rd, #imm8 (0xe3a00000)
        if ((insn & 0x0ff00000) == 0x03a00000) {
            const size_t rd = (insn >> 12) & 0xf;
            const uint32_t imm8 = insn & 0xff;
            ctx32_.r[rd] = imm8;
            return CpuHaltReason::Halted;
        }

        // ADD / SUB Rd, Rn, Rm (0xe0800000 / 0xe0400000)
        if ((insn & 0x0fc00000) == 0x00800000 || (insn & 0x0fc00000) == 0x00400000) {
            const bool is_sub = (insn & 0x0fc00000) == 0x00400000;
            const size_t rn = (insn >> 16) & 0xf;
            const size_t rd = (insn >> 12) & 0xf;
            const size_t rm = insn & 0xf;

            const uint32_t val_n = ctx32_.r[rn];
            const uint32_t val_m = ctx32_.r[rm];
            ctx32_.r[rd] = is_sub ? (val_n - val_m) : (val_n + val_m);
            return CpuHaltReason::Halted;
        }

        LogWarn("Cpu", "Unhandled ARM32 instruction 0x{:08x} at pc=0x{:08x}", insn, current_pc);
        return CpuHaltReason::IllegalInstruction;
    }

    Arch arch_;
    GuestMemory& memory_;
    CpuContext32 ctx32_{};
    CpuContext64 ctx64_{};
    SvcHandler svc_handler_;
    bool running_ = false;
};

} // namespace

std::unique_ptr<GuestCpu> CreateGuestCpu(Arch arch, GuestMemory& memory) {
    return std::make_unique<InterpreterCpu>(arch, memory);
}

} // namespace chronos
