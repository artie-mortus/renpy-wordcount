#pragma once

#include "core/parser.h"

#include <map>
#include <optional>
#include <string>

namespace say_count {

struct ProjectTarget {
    long words = 0;
    long lines = 0;
};

struct ProjectBundleSettings {
    std::string target;
    std::string line_target;
    bool count_menu_choices = false;
    std::string theme = "light";
};

struct ProjectBundle {
    std::vector<NamedScript> files;
    std::size_t active_file = 0;
    ProjectBundleSettings settings;
    std::map<std::string, ProjectTarget> speaker_targets;
    std::map<std::string, ProjectTarget> scene_targets;
    std::string exported_at;
    std::map<std::string, std::string> extra_fields;
};

struct ProjectBundleParseResult {
    std::optional<ProjectBundle> bundle;
    std::string error;
};

ProjectBundleParseResult parse_project_bundle(std::string_view json);
std::string project_bundle_json(const ProjectBundle& bundle);

}  // namespace say_count
