#pragma once

#include <optional>
#include <string>
#include <vector>

namespace say_count {

enum class RenpySdkSource { Configured, Environment, Path, Conventional };

struct RenpySdk {
    std::string executable;
    std::string version;
    RenpySdkSource source = RenpySdkSource::Configured;
};

struct RenpyDetectionOptions {
    std::string configured;
    std::string environment_executable;
    std::vector<std::string> path_entries;
    std::string home;
};

bool is_renpy_executable(const std::string& path);
std::optional<RenpySdk> detect_renpy_sdk(const RenpyDetectionOptions& options);
std::string parse_renpy_version(std::string_view text);
std::string infer_renpy_version(std::string_view executable);

struct RenpyCapabilities {
    bool warp = false;
    bool director = false;
};

RenpyCapabilities renpy_capabilities(std::string_view version);

}  // namespace say_count
