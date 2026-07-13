#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "core/parser.h"

namespace say_count {

enum class AccessibilitySeverity { notice, warning };
enum class AccessibilityKind { image_description, translation, visual_voice, event_description };

struct AccessibilityIssue {
    std::string id;
    AccessibilitySeverity severity = AccessibilitySeverity::notice;
    AccessibilityKind kind = AccessibilityKind::image_description;
    std::string file;
    std::size_t line = 0;
    std::string message;
    bool acknowledged = false;
};

std::vector<AccessibilityIssue> audit_accessibility(
    const std::vector<NamedScript>& scripts,
    const std::set<std::string>& acknowledged = {});

class AccessibilityAcknowledgementStore {
public:
    explicit AccessibilityAcknowledgementStore(std::string path) : path_(std::move(path)) {}
    std::set<std::string> Load() const;
    bool Save(const std::set<std::string>& ids, std::string* error = nullptr) const;
private:
    std::string path_;
};

}  // namespace say_count
