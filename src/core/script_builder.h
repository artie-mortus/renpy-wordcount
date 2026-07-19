#pragma once

#include <string>

namespace say_count {

enum class StoryElementKind { Character, Dialogue, Choice, Scene, Music, Sound, Jump };

struct StoryElementRequest {
    StoryElementKind kind = StoryElementKind::Dialogue;
    std::string primary;
    std::string secondary;
    std::string details;
    std::string indentation;
};

struct StoryElementResult {
    bool valid = false;
    std::string text;
    std::string error;
    std::string note;
};

StoryElementResult build_story_element(const StoryElementRequest& request);

}  // namespace say_count
