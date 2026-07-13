#include "core/renpy.h"

#include <algorithm>
#include <filesystem>
#include <regex>

namespace say_count {
namespace {

namespace fs = std::filesystem;

std::optional<fs::path> Resolve(std::string value) {
    if (value.empty()) return std::nullopt;
    fs::path path(value);
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        for (const char* name : {"renpy.sh", "renpy", "renpy.exe"}) {
            const fs::path candidate = path / name;
            if (is_renpy_executable(candidate.string())) return fs::absolute(candidate, ec);
        }
        return std::nullopt;
    }
    return is_renpy_executable(path.string()) ? std::optional<fs::path>(fs::absolute(path, ec))
                                               : std::nullopt;
}

std::optional<RenpySdk> Candidate(const std::string& value, RenpySdkSource source) {
    const auto path = Resolve(value);
    if (!path) return std::nullopt;
    return RenpySdk{path->lexically_normal().string(), infer_renpy_version(path->string()), source};
}

}  // namespace

bool is_renpy_executable(const std::string& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
#ifdef _WIN32
    return fs::path(path).extension() == ".exe" || fs::path(path).extension() == ".bat";
#else
    const auto permissions = fs::status(path, ec).permissions();
    if (ec) return false;
    using P = fs::perms;
    return (permissions & (P::owner_exec | P::group_exec | P::others_exec)) != P::none;
#endif
}

std::string parse_renpy_version(std::string_view text) {
    const std::regex expression(R"((?:Ren'Py\s+)?(\d+\.\d+(?:\.\d+)?(?:\.\d+)?))",
                                std::regex::icase);
    std::match_results<std::string_view::const_iterator> match;
    return std::regex_search(text.begin(), text.end(), match, expression) ? match[1].str() : std::string{};
}

std::string infer_renpy_version(std::string_view executable) {
    const std::regex expression(R"(renpy-(\d+\.\d+(?:\.\d+)?(?:\.\d+)?)-sdk)",
                                std::regex::icase);
    std::match_results<std::string_view::const_iterator> match;
    return std::regex_search(executable.begin(), executable.end(), match, expression)
        ? match[1].str() : std::string{};
}

std::optional<RenpySdk> detect_renpy_sdk(const RenpyDetectionOptions& options) {
    if (const auto found = Candidate(options.configured, RenpySdkSource::Configured)) return found;
    if (const auto found = Candidate(options.environment_executable, RenpySdkSource::Environment)) return found;
    for (const auto& entry : options.path_entries)
        for (const char* name : {"renpy", "renpy.sh", "renpy.exe"})
            if (const auto found = Candidate((fs::path(entry) / name).string(), RenpySdkSource::Path)) return found;
    if (!options.home.empty()) {
        std::vector<fs::path> candidates;
        std::error_code ec;
        for (const auto& base : {fs::path(options.home), fs::path(options.home) / "Downloads"}) {
            if (!fs::is_directory(base, ec)) continue;
            for (fs::directory_iterator item(base, fs::directory_options::skip_permission_denied, ec), end;
                 item != end; item.increment(ec)) {
                const std::string name = item->path().filename().string();
                if (item->is_directory() && name.rfind("renpy-", 0) == 0 && name.find("-sdk") != std::string::npos)
                    candidates.push_back(item->path());
            }
        }
        std::sort(candidates.rbegin(), candidates.rend());
        for (const auto& candidate : candidates)
            if (const auto found = Candidate(candidate.string(), RenpySdkSource::Conventional)) return found;
    }
    return std::nullopt;
}

RenpyCapabilities renpy_capabilities(std::string_view version) {
    if (version.empty()) return {true, true};
    std::vector<int> parts;
    std::size_t start = 0;
    while (start < version.size()) {
        const std::size_t end = version.find('.', start);
        const auto token = version.substr(start, end == std::string_view::npos ? version.size() - start
                                                                              : end - start);
        try { parts.push_back(std::stoi(std::string(token))); } catch (...) { return {}; }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    if (parts.empty()) return {};
    const int major = parts[0];
    const int minor = parts.size() > 1 ? parts[1] : 0;
    const bool supported = major >= 7 || (major == 6 && minor >= 99);
    return {supported, supported};
}

}  // namespace say_count
