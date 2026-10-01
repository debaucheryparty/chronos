#include <cstdlib>
#include <iostream>
#include <string_view>

#include "runtime/config.h"
#include "runtime/logging.h"
#include "runtime/types.h"

namespace {

constexpr std::string_view kVersion = "0.1.0-dev";

void PrintUsage(std::string_view program_name) {
    std::cout << "Chronos Android Compatibility Runtime (v" << kVersion << ")\n"
              << "Usage: " << program_name << " <command> [options]\n\n"
              << "Commands:\n"
              << "  run <app.apk>             Install and execute an APK directly\n"
              << "  install <app.apk>         Install an APK into runtime storage\n"
              << "  launch <package_name>     Launch an already installed package\n"
              << "  list                      List all installed packages\n"
              << "  uninstall <package_name>  Remove an installed package\n\n"
              << "Options:\n"
              << "  --arch <arm32|arm64>      Target guest architecture (default: arm64)\n"
              << "  --log-level <level>       Set log level (trace, debug, info, warn, error)\n"
              << "  --trace-svc               Enable SVC / system call tracing\n"
              << "  -h, --help                Display this help message\n"
              << "  -v, --version             Display version information\n";
}

void PrintVersion() {
    std::cout << "chronos " << kVersion << " (Linux x86_64 host, C++20)\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    std::string_view command = argv[1];

    if (command == "-h" || command == "--help") {
        PrintUsage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (command == "-v" || command == "--version") {
        PrintVersion();
        return EXIT_SUCCESS;
    }

    chronos::RuntimeConfig config;

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--arch" && i + 1 < argc) {
            std::string_view arch_str = argv[++i];
            if (arch_str == "arm32") {
                config.target_arch = chronos::Arch::Arm32;
            } else if (arch_str == "arm64") {
                config.target_arch = chronos::Arch::Arm64;
            } else {
                std::cerr << "Error: Unknown architecture '" << arch_str << "' (expected arm32 or arm64)\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--log-level" && i + 1 < argc) {
            std::string_view level_str = argv[++i];
            if (level_str == "trace") {
                config.log_level = chronos::LogLevel::Trace;
            } else if (level_str == "debug") {
                config.log_level = chronos::LogLevel::Debug;
            } else if (level_str == "info") {
                config.log_level = chronos::LogLevel::Info;
            } else if (level_str == "warn") {
                config.log_level = chronos::LogLevel::Warn;
            } else if (level_str == "error") {
                config.log_level = chronos::LogLevel::Error;
            }
        } else if (arg == "--trace-svc") {
            config.enable_svc_tracing = true;
        } else if (config.package_path.empty() && !arg.starts_with("-")) {
            config.package_path = arg;
        }
    }

    chronos::Logger::Instance().SetLevel(config.log_level);

    if (command == "run") {
        if (config.package_path.empty()) {
            std::cerr << "Error: 'run' command requires an APK path.\n";
            return EXIT_FAILURE;
        }
        chronos::LogInfo("Runtime", "Starting execution of package: {}", config.package_path.string());
        chronos::LogInfo("Runtime", "Target guest ABI: {}", chronos::ArchToString(config.target_arch));
        // Phase 1+ integration will be hooked here
        return EXIT_SUCCESS;
    }

    if (command == "install") {
        if (config.package_path.empty()) {
            std::cerr << "Error: 'install' command requires an APK path.\n";
            return EXIT_FAILURE;
        }
        chronos::LogInfo("PackageManager", "Installing package: {}", config.package_path.string());
        return EXIT_SUCCESS;
    }

    if (command == "launch") {
        if (config.package_path.empty()) {
            std::cerr << "Error: 'launch' command requires a package name.\n";
            return EXIT_FAILURE;
        }
        chronos::LogInfo("Runtime", "Launching installed package: {}", config.package_path.string());
        return EXIT_SUCCESS;
    }

    if (command == "list") {
        chronos::LogInfo("PackageManager", "No installed packages found.");
        return EXIT_SUCCESS;
    }

    if (command == "uninstall") {
        if (config.package_path.empty()) {
            std::cerr << "Error: 'uninstall' command requires a package name.\n";
            return EXIT_FAILURE;
        }
        chronos::LogInfo("PackageManager", "Uninstalling package: {}", config.package_path.string());
        return EXIT_SUCCESS;
    }

    std::cerr << "Error: Unknown command '" << command << "'. Run with --help for usage.\n";
    return EXIT_FAILURE;
}
