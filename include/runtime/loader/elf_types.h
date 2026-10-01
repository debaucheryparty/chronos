#pragma once

#include <cstdint>

namespace chronos::elf {

constexpr uint8_t kElfMag0 = 0x7f;
constexpr uint8_t kElfMag1 = 'E';
constexpr uint8_t kElfMag2 = 'L';
constexpr uint8_t kElfMag3 = 'F';

constexpr uint8_t kElfClassNone = 0;
constexpr uint8_t kElfClass32 = 1;
constexpr uint8_t kElfClass64 = 2;

constexpr uint8_t kElfDataNone = 0;
constexpr uint8_t kElfData2Lsb = 1;
constexpr uint8_t kElfData2Msb = 2;

constexpr uint16_t kEtNone = 0;
constexpr uint16_t kEtRel = 1;
constexpr uint16_t kEtExec = 2;
constexpr uint16_t kEtDyn = 3;
constexpr uint16_t kEtCore = 4;

constexpr uint16_t kEmArm = 40;
constexpr uint16_t kEmAarch64 = 183;

constexpr uint32_t kPtNull = 0;
constexpr uint32_t kPtLoad = 1;
constexpr uint32_t kPtDynamic = 2;
constexpr uint32_t kPtInterp = 3;
constexpr uint32_t kPtNote = 4;
constexpr uint32_t kPtShlib = 5;
constexpr uint32_t kPtPhdr = 6;
constexpr uint32_t kPtTls = 7;
constexpr uint32_t kPtGnuEhFrame = 0x6474e550;
constexpr uint32_t kPtGnuStack = 0x6474e551;
constexpr uint32_t kPtGnuRelro = 0x6474e552;
constexpr uint32_t kPtArmExidx = 0x70000001;

constexpr uint32_t kPfX = 1 << 0;
constexpr uint32_t kPfW = 1 << 1;
constexpr uint32_t kPfR = 1 << 2;

constexpr int64_t kDtNull = 0;
constexpr int64_t kDtNeeded = 1;
constexpr int64_t kDtPltRelSz = 2;
constexpr int64_t kDtPltGot = 3;
constexpr int64_t kDtHash = 4;
constexpr int64_t kDtStrTab = 5;
constexpr int64_t kDtSymTab = 6;
constexpr int64_t kDtRela = 7;
constexpr int64_t kDtRelaSz = 8;
constexpr int64_t kDtRelaEnt = 9;
constexpr int64_t kDtStrSz = 10;
constexpr int64_t kDtSymEnt = 11;
constexpr int64_t kDtInit = 12;
constexpr int64_t kDtFini = 13;
constexpr int64_t kDtSoName = 14;
constexpr int64_t kDtRPath = 15;
constexpr int64_t kDtSymbolic = 16;
constexpr int64_t kDtRel = 17;
constexpr int64_t kDtRelSz = 18;
constexpr int64_t kDtRelEnt = 19;
constexpr int64_t kDtPltRel = 20;
constexpr int64_t kDtJmpRel = 23;
constexpr int64_t kDtInitArray = 25;
constexpr int64_t kDtFiniArray = 26;
constexpr int64_t kDtInitArraySz = 27;
constexpr int64_t kDtFiniArraySz = 28;
constexpr int64_t kDtFlags = 30;
constexpr int64_t kDtFlags1 = 0x6ffffffb;
constexpr int64_t kDtGnuHash = 0x6ffffef5;

constexpr uint32_t kRArmNone = 0;
constexpr uint32_t kRArmAbs32 = 2;
constexpr uint32_t kRArmRel32 = 3;
constexpr uint32_t kRArmGlobDat = 21;
constexpr uint32_t kRArmJumpSlot = 22;
constexpr uint32_t kRArmRelative = 23;

constexpr uint32_t kRAarch64None = 0;
constexpr uint32_t kRAarch64Abs64 = 257;
constexpr uint32_t kRAarch64Copy = 1024;
constexpr uint32_t kRAarch64GlobDat = 1025;
constexpr uint32_t kRAarch64JumpSlot = 1026;
constexpr uint32_t kRAarch64Relative = 1027;

struct Elf32_Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct Elf32_Dyn {
    int32_t d_tag;
    union {
        uint32_t d_val;
        uint32_t d_ptr;
    } d_un;
};

struct Elf64_Dyn {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
};

struct Elf32_Sym {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf32_Rel {
    uint32_t r_offset;
    uint32_t r_info;
};

struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
};

inline uint32_t Elf32RType(uint32_t info) noexcept {
    return info & 0xff;
}

inline uint32_t Elf32RSym(uint32_t info) noexcept {
    return info >> 8;
}

inline uint32_t Elf64RType(uint64_t info) noexcept {
    return static_cast<uint32_t>(info & 0xffffffff);
}

inline uint32_t Elf64RSym(uint64_t info) noexcept {
    return static_cast<uint32_t>(info >> 32);
}

inline uint8_t ElfSymBind(uint8_t info) noexcept {
    return info >> 4;
}

inline uint8_t ElfSymType(uint8_t info) noexcept {
    return info & 0xf;
}

} // namespace chronos::elf
