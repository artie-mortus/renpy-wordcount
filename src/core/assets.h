#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace say_count {

enum class AssetKind { Image, Audio };
struct ProjectAsset { std::string relative_path; std::string absolute_path; AssetKind kind; };
enum class AssetInsertAction { Show, Scene, PlayMusic, PlaySound };

std::vector<ProjectAsset> discover_project_assets(const std::string& game_directory,
                                                  std::size_t limit = 5000);
std::string asset_insert_statement(const ProjectAsset& asset, AssetInsertAction action);

}  // namespace say_count
