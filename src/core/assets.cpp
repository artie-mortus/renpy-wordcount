#include "core/assets.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>

namespace say_count {
namespace {
namespace fs = std::filesystem;

std::string Lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
}

std::vector<ProjectAsset> discover_project_assets(const std::string& game_directory, std::size_t limit) {
    static const std::unordered_map<std::string, AssetKind> extensions{
        {".png", AssetKind::Image}, {".jpg", AssetKind::Image}, {".jpeg", AssetKind::Image},
        {".webp", AssetKind::Image}, {".gif", AssetKind::Image}, {".ogg", AssetKind::Audio},
        {".mp3", AssetKind::Audio}, {".wav", AssetKind::Audio}, {".opus", AssetKind::Audio},
        {".flac", AssetKind::Audio}};
    std::vector<ProjectAsset> assets;
    std::error_code ec;
    fs::recursive_directory_iterator iterator(game_directory, fs::directory_options::skip_permission_denied, ec), end;
    for (; iterator != end && assets.size() < limit; iterator.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (iterator->is_directory(ec)) {
            const std::string name = iterator->path().filename().string();
            const std::string lowered = Lower(name);
            if ((!name.empty() && name.front() == '.') || lowered == "cache" || lowered == "saves" || lowered == "tl")
                iterator.disable_recursion_pending();
            continue;
        }
        if (!iterator->is_regular_file(ec)) continue;
        const auto found = extensions.find(Lower(iterator->path().extension().string()));
        if (found == extensions.end()) continue;
        const auto relative = fs::relative(iterator->path(), game_directory, ec);
        if (ec) { ec.clear(); continue; }
        assets.push_back({relative.generic_string(), iterator->path().string(), found->second});
    }
    std::sort(assets.begin(), assets.end(), [](const auto& left, const auto& right) {
        return left.relative_path < right.relative_path;
    });
    return assets;
}

std::string asset_insert_statement(const ProjectAsset& asset, AssetInsertAction action) {
    if (action == AssetInsertAction::PlayMusic)
        return "play music \"" + asset.relative_path + "\"";
    if (action == AssetInsertAction::PlaySound)
        return "play sound \"" + asset.relative_path + "\"";
    std::string stem = fs::path(asset.relative_path).replace_extension().generic_string();
    for (char& c : stem) if (c == '/' || c == '\\' || c == '_' || c == '-') c = ' ';
    return std::string(action == AssetInsertAction::Scene ? "scene " : "show ") + stem;
}

}  // namespace say_count
