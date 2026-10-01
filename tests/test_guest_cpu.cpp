#include "runtime/cpu/guest_cpu.h"
#include "runtime/memory/guest_memory.h"
#include "test_framework.h"

#include <vector>

namespace chronos {

TEST_CASE(AArch64ExecuteArithmeticAndReturn) {
    GuestMemory mem(Arch::Arm64);
    const GuestAddress code_addr = 0x1000'0000;
    auto map_res = mem.Map(code_addr, 0x1000, MemoryPermission::ReadWriteExecute);
    ASSERT_TRUE(map_res.has_value());

    // AArch64 machine instructions:
    // 0xd2800540: movz x0, #42
    // 0xd2800101: movz x1, #8
    // 0x8b010000: add x0, x0, x1
    // 0xd65f03c0: ret
    const std::vector<uint32_t> code = {
        0xd2800540,
        0xd2800101,
        0x8b010000,
        0xd65f03c0,
    };

    auto write_res = mem.Write(code_addr, code.data(), code.size() * sizeof(uint32_t));
    ASSERT_TRUE(write_res.has_value());

    auto cpu = CreateGuestCpu(Arch::Arm64, mem);
    ASSERT_TRUE(cpu != nullptr);

    CpuContext64 ctx{};
    ctx.pc = code_addr;
    ctx.set_lr(0x0); // Return address halts execution
    cpu->LoadContext(ctx);

    auto run_res = cpu->Run();
    ASSERT_TRUE(run_res.has_value());
    ASSERT_EQ(run_res.value(), CpuHaltReason::Halted);

    CpuContext64 result_ctx{};
    cpu->SaveContext(result_ctx);
    ASSERT_EQ(result_ctx.x[0], 50ULL);
}

TEST_CASE(AArch64SvcDispatch) {
    GuestMemory mem(Arch::Arm64);
    const GuestAddress code_addr = 0x1000'0000;
    auto map_res = mem.Map(code_addr, 0x1000, MemoryPermission::ReadWriteExecute);
    ASSERT_TRUE(map_res.has_value());

    // 0xd2800ba8: movz x8, #93 (exit_group syscall number on Linux/Android ARM64)
    // 0xd4000001: svc #0
    const std::vector<uint32_t> code = {
        0xd2800ba8,
        0xd4000001,
    };

    auto write_res = mem.Write(code_addr, code.data(), code.size() * sizeof(uint32_t));
    ASSERT_TRUE(write_res.has_value());

    auto cpu = CreateGuestCpu(Arch::Arm64, mem);

    uint32_t captured_swi = 0xffff;
    cpu->SetSvcHandler([&](uint32_t swi) {
        captured_swi = swi;
        cpu->Stop();
    });

    CpuContext64 ctx{};
    ctx.pc = code_addr;
    cpu->LoadContext(ctx);

    auto run_res = cpu->Run();
    ASSERT_TRUE(run_res.has_value());
    ASSERT_EQ(run_res.value(), CpuHaltReason::Svc);
    ASSERT_EQ(captured_swi, 0u);

    CpuContext64 result_ctx{};
    cpu->SaveContext(result_ctx);
    ASSERT_EQ(result_ctx.x[8], 93ULL);
}

TEST_CASE(Arm32ExecuteArithmeticAndReturn) {
    GuestMemory mem(Arch::Arm32);
    const GuestAddress code_addr = 0x2000'0000;
    auto map_res = mem.Map(code_addr, 0x1000, MemoryPermission::ReadWriteExecute);
    ASSERT_TRUE(map_res.has_value());

    // ARM32 machine instructions:
    // 0xe3a00014: mov r0, #20
    // 0xe3a01005: mov r1, #5
    // 0xe0400001: sub r0, r0, r1
    // 0xe12fff1e: bx lr
    const std::vector<uint32_t> code = {
        0xe3a00014,
        0xe3a01005,
        0xe0400001,
        0xe12fff1e,
    };

    auto write_res = mem.Write(code_addr, code.data(), code.size() * sizeof(uint32_t));
    ASSERT_TRUE(write_res.has_value());

    auto cpu = CreateGuestCpu(Arch::Arm32, mem);

    CpuContext32 ctx{};
    ctx.set_pc(static_cast<uint32_t>(code_addr));
    ctx.set_lr(0x0); // Return address halts execution
    cpu->LoadContext(ctx);

    auto run_res = cpu->Run();
    ASSERT_TRUE(run_res.has_value());
    ASSERT_EQ(run_res.value(), CpuHaltReason::Halted);

    CpuContext32 result_ctx{};
    cpu->SaveContext(result_ctx);
    ASSERT_EQ(result_ctx.r[0], 15u);
}

} // namespace chronos
