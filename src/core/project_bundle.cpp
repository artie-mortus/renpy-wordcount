#include "core/project_bundle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <variant>

namespace say_count {
namespace {

struct Json {
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view source) : source_(source) {}
    Json Parse() {
        Json result = Value();
        Space();
        if (position_ != source_.size()) throw std::runtime_error("Unexpected trailing JSON data");
        return result;
    }

private:
    void Space() {
        while (position_ < source_.size() &&
               std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
    }
    char Take() {
        if (position_ >= source_.size()) throw std::runtime_error("Unexpected end of JSON");
        return source_[position_++];
    }
    bool Consume(char expected) {
        Space();
        if (position_ < source_.size() && source_[position_] == expected) { ++position_; return true; }
        return false;
    }
    static void Utf8(std::string& out, unsigned value) {
        if (value <= 0x7f) out.push_back(static_cast<char>(value));
        else if (value <= 0x7ff) {
            out.push_back(static_cast<char>(0xc0 | (value >> 6)));
            out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xe0 | (value >> 12)));
            out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
    }
    std::string String() {
        Space();
        if (Take() != '"') throw std::runtime_error("Expected JSON string");
        std::string result;
        while (true) {
            const char c = Take();
            if (c == '"') return result;
            if (static_cast<unsigned char>(c) < 0x20) throw std::runtime_error("Invalid JSON string");
            if (c != '\\') { result.push_back(c); continue; }
            const char escaped = Take();
            if (escaped == '"' || escaped == '\\' || escaped == '/') result.push_back(escaped);
            else if (escaped == 'b') result.push_back('\b');
            else if (escaped == 'f') result.push_back('\f');
            else if (escaped == 'n') result.push_back('\n');
            else if (escaped == 'r') result.push_back('\r');
            else if (escaped == 't') result.push_back('\t');
            else if (escaped == 'u') {
                unsigned code = 0;
                for (int index = 0; index < 4; ++index) {
                    const char digit = Take();
                    code <<= 4;
                    if (digit >= '0' && digit <= '9') code += digit - '0';
                    else if (digit >= 'a' && digit <= 'f') code += digit - 'a' + 10;
                    else if (digit >= 'A' && digit <= 'F') code += digit - 'A' + 10;
                    else throw std::runtime_error("Invalid JSON Unicode escape");
                }
                Utf8(result, code);
            } else throw std::runtime_error("Invalid JSON escape");
        }
    }
    Json Value() {
        Space();
        if (position_ >= source_.size()) throw std::runtime_error("Expected JSON value");
        if (source_[position_] == '"') return Json{String()};
        if (source_[position_] == '{') {
            ++position_;
            Json::Object object;
            Space();
            if (Consume('}')) return Json{object};
            while (true) {
                std::string key = String();
                if (!Consume(':')) throw std::runtime_error("Expected ':' in JSON object");
                if (!object.emplace(std::move(key), Value()).second)
                    throw std::runtime_error("Duplicate JSON object key");
                if (Consume('}')) return Json{object};
                if (!Consume(',')) throw std::runtime_error("Expected ',' in JSON object");
            }
        }
        if (source_[position_] == '[') {
            ++position_;
            Json::Array array;
            Space();
            if (Consume(']')) return Json{array};
            while (true) {
                array.push_back(Value());
                if (Consume(']')) return Json{array};
                if (!Consume(',')) throw std::runtime_error("Expected ',' in JSON array");
            }
        }
        for (const auto& literal : {std::pair{"true", Json{true}}, std::pair{"false", Json{false}},
                                    std::pair{"null", Json{nullptr}}}) {
            const std::string_view word(literal.first);
            if (source_.substr(position_, word.size()) == word) {
                position_ += word.size();
                return literal.second;
            }
        }
        const std::size_t begin = position_;
        if (source_[position_] == '-') ++position_;
        while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) ++position_;
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) ++position_;
        }
        if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
            while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) ++position_;
        }
        if (begin == position_) throw std::runtime_error("Invalid JSON value");
        const std::string number(source_.substr(begin, position_ - begin));
        std::size_t consumed = 0;
        const double parsed = std::stod(number, &consumed);
        if (consumed != number.size() || !std::isfinite(parsed)) throw std::runtime_error("Invalid JSON number");
        return Json{parsed};
    }

    std::string_view source_;
    std::size_t position_ = 0;
};

std::string Escape(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
                else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}

std::string Compact(const Json& json) {
    if (std::holds_alternative<std::nullptr_t>(json.value)) return "null";
    if (const auto* value = std::get_if<bool>(&json.value)) return *value ? "true" : "false";
    if (const auto* value = std::get_if<double>(&json.value)) {
        std::ostringstream out; out << std::setprecision(17) << *value; return out.str();
    }
    if (const auto* value = std::get_if<std::string>(&json.value)) return Escape(*value);
    if (const auto* values = std::get_if<Json::Array>(&json.value)) {
        std::string out = "[";
        for (std::size_t index = 0; index < values->size(); ++index) {
            if (index) out += ',';
            out += Compact((*values)[index]);
        }
        return out + ']';
    }
    std::string out = "{";
    const auto& values = std::get<Json::Object>(json.value);
    std::size_t index = 0;
    for (const auto& [key, value] : values) {
        if (index++) out += ',';
        out += Escape(key) + ':' + Compact(value);
    }
    return out + '}';
}

const Json* Member(const Json::Object& object, const char* key) {
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
}

const std::string* StringMember(const Json::Object& object, const char* key) {
    const auto* value = Member(object, key);
    return value ? std::get_if<std::string>(&value->value) : nullptr;
}

std::map<std::string, ProjectTarget> ReadTargets(const Json& json) {
    const auto* object = std::get_if<Json::Object>(&json.value);
    if (!object) throw std::runtime_error("Project bundle contains invalid targets");
    std::map<std::string, ProjectTarget> result;
    for (const auto& [name, raw] : *object) {
        const auto* values = std::get_if<Json::Object>(&raw.value);
        if (!values) continue;
        ProjectTarget target;
        if (const auto* words = Member(*values, "words"))
            if (const auto* number = std::get_if<double>(&words->value)) target.words = static_cast<long>(*number);
        if (const auto* lines = Member(*values, "lines"))
            if (const auto* number = std::get_if<double>(&lines->value)) target.lines = static_cast<long>(*number);
        result[name] = target;
    }
    return result;
}

void WriteTargets(std::ostringstream& out, const std::map<std::string, ProjectTarget>& targets,
                  int indent) {
    out << "{";
    std::size_t index = 0;
    for (const auto& [name, target] : targets) {
        out << (index++ ? ",\n" : "\n") << std::string(indent + 2, ' ') << Escape(name) << ": {";
        bool field = false;
        if (target.words) { out << "\"words\": " << target.words; field = true; }
        if (target.lines) { if (field) out << ", "; out << "\"lines\": " << target.lines; }
        out << "}";
    }
    if (!targets.empty()) out << '\n' << std::string(indent, ' ');
    out << "}";
}

}  // namespace

ProjectBundleParseResult parse_project_bundle(std::string_view json) {
    ProjectBundleParseResult result;
    try {
        const Json root = JsonParser(json).Parse();
        const auto* object = std::get_if<Json::Object>(&root.value);
        if (!object) throw std::runtime_error("Not a Say Count project bundle");
        const auto* format = StringMember(*object, "format");
        const auto* version = Member(*object, "version");
        const auto* version_number = version ? std::get_if<double>(&version->value) : nullptr;
        const auto* raw_files = Member(*object, "files");
        const auto* files = raw_files ? std::get_if<Json::Array>(&raw_files->value) : nullptr;
        if (!format || *format != "say-count-project" || !version_number || *version_number != 1 || !files)
            throw std::runtime_error("Not a Say Count project bundle");
        ProjectBundle bundle;
        for (const auto& raw_file : *files) {
            const auto* file = std::get_if<Json::Object>(&raw_file.value);
            const auto* name = file ? StringMember(*file, "name") : nullptr;
            const auto* content = file ? StringMember(*file, "content") : nullptr;
            if (!name || !content) throw std::runtime_error("Project bundle contains invalid files");
            bundle.files.push_back({*name, *content});
        }
        if (bundle.files.empty()) throw std::runtime_error("Project bundle contains invalid files");
        if (const auto* active = Member(*object, "activeFile")) {
            if (const auto* number = std::get_if<double>(&active->value); number && *number >= 0)
                bundle.active_file = std::min(static_cast<std::size_t>(*number), bundle.files.size() - 1);
        }
        const auto* raw_settings = Member(*object, "settings");
        const auto* settings = raw_settings ? std::get_if<Json::Object>(&raw_settings->value) : nullptr;
        const auto* target = settings ? StringMember(*settings, "target") : nullptr;
        const auto* line_target = settings ? StringMember(*settings, "lineTarget") : nullptr;
        const auto* raw_menu = settings ? Member(*settings, "countMenuChoices") : nullptr;
        const auto* menu = raw_menu ? std::get_if<bool>(&raw_menu->value) : nullptr;
        const auto* theme = settings ? StringMember(*settings, "theme") : nullptr;
        if (!target || !line_target || !menu || !theme || (*theme != "light" && *theme != "dark"))
            throw std::runtime_error("Project bundle contains invalid settings");
        bundle.settings = {*target, *line_target, *menu, *theme};
        const auto* raw_targets = Member(*object, "targets");
        const auto* targets = raw_targets ? std::get_if<Json::Object>(&raw_targets->value) : nullptr;
        const auto* speakers = targets ? Member(*targets, "speakerTargets") : nullptr;
        const auto* scenes = targets ? Member(*targets, "sceneTargets") : nullptr;
        if (!speakers || !scenes) throw std::runtime_error("Project bundle contains invalid targets");
        bundle.speaker_targets = ReadTargets(*speakers);
        bundle.scene_targets = ReadTargets(*scenes);
        if (const auto* exported = StringMember(*object, "exportedAt")) bundle.exported_at = *exported;
        for (const auto& [key, value] : *object) {
            if (key != "format" && key != "version" && key != "exportedAt" && key != "files" &&
                key != "activeFile" && key != "settings" && key != "targets")
                bundle.extra_fields[key] = Compact(value);
        }
        result.bundle = std::move(bundle);
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

std::string project_bundle_json(const ProjectBundle& bundle) {
    std::ostringstream out;
    out << "{\n  \"format\": \"say-count-project\",\n  \"version\": 1,\n"
        << "  \"exportedAt\": " << Escape(bundle.exported_at) << ",\n  \"files\": [";
    for (std::size_t index = 0; index < bundle.files.size(); ++index) {
        const auto& file = bundle.files[index];
        out << (index ? ",\n" : "\n") << "    {\"name\": " << Escape(file.name)
            << ", \"content\": " << Escape(file.content) << "}";
    }
    if (!bundle.files.empty()) out << '\n';
    out << "  ],\n  \"activeFile\": " << std::min(bundle.active_file,
        bundle.files.empty() ? std::size_t{0} : bundle.files.size() - 1)
        << ",\n  \"settings\": {\n"
        << "    \"target\": " << Escape(bundle.settings.target) << ",\n"
        << "    \"lineTarget\": " << Escape(bundle.settings.line_target) << ",\n"
        << "    \"countMenuChoices\": " << (bundle.settings.count_menu_choices ? "true" : "false") << ",\n"
        << "    \"theme\": " << Escape(bundle.settings.theme) << "\n  },\n"
        << "  \"targets\": {\n    \"speakerTargets\": ";
    WriteTargets(out, bundle.speaker_targets, 4);
    out << ",\n    \"sceneTargets\": ";
    WriteTargets(out, bundle.scene_targets, 4);
    out << "\n  }";
    for (const auto& [key, value] : bundle.extra_fields)
        out << ",\n  " << Escape(key) << ": " << value;
    out << "\n}\n";
    return out.str();
}

}  // namespace say_count
