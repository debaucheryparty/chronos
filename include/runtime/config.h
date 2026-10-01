#pragma once

#include <filesystem>
#include <string>

#include "runtime/logging.h"
#include "runtime/types.h"

namespace chronos {

struct RuntimeConfig {
    std::filesystem::path package_path;
    std::filesystem::path data_directory = "data";
    Arch target_arch = Arch::Arm64;
    LogLevel log_level = LogLevel::Info;
    bool enable_svc_tracing = false;
    bool enable_graphics = true;
};

} // namespace chronos
