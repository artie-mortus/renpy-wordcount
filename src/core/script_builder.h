#pragma once

#include <string>
#include <utility>

namespace say_count {

enum class StoryElementKind {
    Character,
    Dialogue,
    Choice,
    Scene,
    Music,
    Sound,
    Jump,
    Show,
    Hide,
    StopMusic,
    Pause,
    Call,
    Return,
    Transition,
};

struct StoryElementRequest {
    StoryElementRequest() = default;
    StoryElementRequest(StoryElementKind element_kind, std::string primary_value = {},
                        std::string secondary_value = {}, std::string details_value = {},
                        std::string indentation_value = {})
        : kind(element_kind), primary(std::move(primary_value)),
          secondary(std::move(secondary_value)), details(std::move(details_value)),
          indentation(std::move(indentation_value)) {}

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
