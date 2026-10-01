#include "runtime/cpu/guest_cpu.h"
#include "runtime/loader/elf_image.h"
#include "runtime/loader/elf_loader.h"
#include "runtime/loader/elf_types.h"
#include "runtime/memory/guest_memory.h"
#include "test_framework.h"

#include <cstring>
#include <vector>

namespace chronos {

namespace {

std::vector<uint8_t> CreateMinimalAArch64Elf() {
    std::vector<uint8_t> buffer(4096, 0);

    auto* ehdr = reinterpret_cast<elf::Elf64_Ehdr*>(buffer.data());
    ehdr->e_ident[0] = elf::kElfMag0;
    ehdr->e_ident[1] = elf::kElfMag1;
    ehdr->e_ident[2] = elf::kElfMag2;
    ehdr->e_ident[3] = elf::kElfMag3;
    ehdr->e_ident[4] = elf::kElfClass64;
    ehdr->e_ident[5] = elf::kElfData2Lsb;
    ehdr->e_type = elf::kEtDyn;
    ehdr->e_machine = elf::kEmAarch64;
    ehdr->e_version = 1;
    ehdr->e_entry = 0x1000;
    ehdr->e_phoff = sizeof(elf::Elf64_Ehdr);
    ehdr->e_ehsize = sizeof(elf::Elf64_Ehdr);
    ehdr->e_phentsize = sizeof(elf::Elf64_Phdr);
    ehdr->e_phnum = 1;

    auto* phdr = reinterpret_cast<elf::Elf64_Phdr*>(buffer.data() + ehdr->e_phoff);
    phdr->p_type = elf::kPtLoad;
    phdr->p_flags = elf::kPfR | elf::kPfW | elf::kPfX;
    phdr->p_offset = 0x1000;
    phdr->p_vaddr = 0x1000;
    phdr->p_paddr = 0x1000;
    phdr->p_filesz = 8;
    phdr->p_memsz = 8;
    phdr->p_align = 4096;

    buffer.resize(0x1000 + 8, 0);

    const uint32_t code[] = {
        0xd2800c80, // movz x0, #100
        0xd65f03c0, // ret
    };
    std::memcpy(buffer.data() + 0x1000, code, sizeof(code));

    return buffer;
}

std::vector<uint8_t> CreateMinimalArm32Elf() {
    std::vector<uint8_t> buffer(4096, 0);

    auto* ehdr = reinterpret_cast<elf::Elf32_Ehdr*>(buffer.data());
    ehdr->e_ident[0] = elf::kElfMag0;
    ehdr->e_ident[1] = elf::kElfMag1;
    ehdr->e_ident[2] = elf::kElfMag2;
    ehdr->e_ident[3] = elf::kElfMag3;
    ehdr->e_ident[4] = elf::kElfClass32;
    ehdr->e_ident[5] = elf::kElfData2Lsb;
    ehdr->e_type = elf::kEtDyn;
    ehdr->e_machine = elf::kEmArm;
    ehdr->e_version = 1;
    ehdr->e_entry = 0x1000;
    ehdr->e_phoff = sizeof(elf::Elf32_Ehdr);
    ehdr->e_ehsize = sizeof(elf::Elf32_Ehdr);
    ehdr->e_phentsize = sizeof(elf::Elf32_Phdr);
    ehdr->e_phnum = 1;

    auto* phdr = reinterpret_cast<elf::Elf32_Phdr*>(buffer.data() + ehdr->e_phoff);
    phdr->p_type = elf::kPtLoad;
    phdr->p_flags = elf::kPfR | elf::kPfW | elf::kPfX;
    phdr->p_offset = 0x1000;
    phdr->p_vaddr = 0x1000;
    phdr->p_paddr = 0x1000;
    phdr->p_filesz = 8;
    phdr->p_memsz = 8;
    phdr->p_align = 4096;

    buffer.resize(0x1000 + 8, 0);

    const uint32_t code[] = {
        0xe3a0002a, // mov r0, #42
        0xe12fff1e, // bx lr
    };
    std::memcpy(buffer.data() + 0x1000, code, sizeof(code));

    return buffer;
}

} // namespace

TEST_CASE(ElfImageParseAArch64) {
    auto data = CreateMinimalAArch64Elf();
    auto res = ElfImage::Parse(data);
    ASSERT_TRUE(res.has_value());

    const auto& img = res.value();
    ASSERT_EQ(img.arch(), Arch::Arm64);
    ASSERT_EQ(img.entry_point(), 0x1000ULL);
    ASSERT_EQ(img.segments().size(), 1u);
    ASSERT_TRUE(img.is_pie());
}

TEST_CASE(ElfImageParseArm32) {
    auto data = CreateMinimalArm32Elf();
    auto res = ElfImage::Parse(data);
    ASSERT_TRUE(res.has_value());

    const auto& img = res.value();
    ASSERT_EQ(img.arch(), Arch::Arm32);
    ASSERT_EQ(img.entry_point(), 0x1000ULL);
    ASSERT_EQ(img.segments().size(), 1u);
    ASSERT_TRUE(img.is_pie());
}

TEST_CASE(ElfLoaderLoadAndExecuteAArch64) {
    auto data = CreateMinimalAArch64Elf();
    auto parse_res = ElfImage::Parse(data);
    ASSERT_TRUE(parse_res.has_value());

    GuestMemory mem(Arch::Arm64);
    ElfLoader loader(mem);

    auto load_res = loader.Load(parse_res.value());
    ASSERT_TRUE(load_res.has_value());

    const auto& lib = load_res.value();
    ASSERT_TRUE(lib.load_bias != 0);
    ASSERT_EQ(lib.entry_point, lib.load_bias + 0x1000);

    auto cpu = CreateGuestCpu(Arch::Arm64, mem);
    CpuContext64 ctx{};
    ctx.pc = lib.entry_point;
    ctx.set_lr(0x0);
    cpu->LoadContext(ctx);

    auto run_res = cpu->Run();
    ASSERT_TRUE(run_res.has_value());
    ASSERT_EQ(run_res.value(), CpuHaltReason::Halted);

    CpuContext64 res_ctx{};
    cpu->SaveContext(res_ctx);
    ASSERT_EQ(res_ctx.x[0], 100ULL);
}

TEST_CASE(ElfLoaderLoadAndExecuteArm32) {
    auto data = CreateMinimalArm32Elf();
    auto parse_res = ElfImage::Parse(data);
    ASSERT_TRUE(parse_res.has_value());

    GuestMemory mem(Arch::Arm32);
    ElfLoader loader(mem);

    auto load_res = loader.Load(parse_res.value());
    ASSERT_TRUE(load_res.has_value());

    const auto& lib = load_res.value();
    ASSERT_TRUE(lib.load_bias != 0);
    ASSERT_EQ(lib.entry_point, lib.load_bias + 0x1000);

    auto cpu = CreateGuestCpu(Arch::Arm32, mem);
    CpuContext32 ctx{};
    ctx.set_pc(static_cast<uint32_t>(lib.entry_point));
    ctx.set_lr(0x0);
    cpu->LoadContext(ctx);

    auto run_res = cpu->Run();
    ASSERT_TRUE(run_res.has_value());
    ASSERT_EQ(run_res.value(), CpuHaltReason::Halted);

    CpuContext32 res_ctx{};
    cpu->SaveContext(res_ctx);
    ASSERT_EQ(res_ctx.r[0], 42u);
}

TEST_CASE(RealAndroidLibraryLoad) {
    const std::filesystem::path lib_path = "C:/Users/sahil/.gemini/antigravity-ide/brain/3b66967d-4e17-49c2-a521-6f6f482843ce/scratch/lunaria/lib/armeabi-v7a/libmain.so";
    if (!std::filesystem::exists(lib_path)) {
        return;
    }

    auto img_res = ElfImage::LoadFromFile(lib_path);
    ASSERT_TRUE(img_res.has_value());
    ASSERT_EQ(img_res.value().arch(), Arch::Arm32);
    ASSERT_TRUE(!img_res.value().segments().empty());

    GuestMemory mem(Arch::Arm32);
    ElfLoader loader(mem);
    auto load_res = loader.Load(img_res.value());
    ASSERT_TRUE(load_res.has_value());
    ASSERT_TRUE(load_res.value().load_bias != 0);
}

} // namespace chronos
