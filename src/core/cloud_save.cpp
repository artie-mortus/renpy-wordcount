#include "core/cloud_save.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>

namespace say_count {
namespace {

std::size_t SkipSpace(std::string_view source, std::size_t position) {
    while (position < source.size() &&
           std::isspace(static_cast<unsigned char>(source[position]))) ++position;
    return position;
}

std::optional<std::string> ParseString(std::string_view source, std::size_t* position) {
    std::size_t index = SkipSpace(source, *position);
    if (index >= source.size() || source[index++] != '"') return std::nullopt;
    std::string result;
    while (index < source.size()) {
        const char value = source[index++];
        if (value == '"') {
            *position = index;
            return result;
        }
        if (value != '\\') {
            result.push_back(value);
            continue;
        }
        if (index >= source.size()) return std::nullopt;
        const char escaped = source[index++];
        if (escaped == '"' || escaped == '\\' || escaped == '/') result.push_back(escaped);
        else if (escaped == 'b') result.push_back('\b');
        else if (escaped == 'f') result.push_back('\f');
        else if (escaped == 'n') result.push_back('\n');
        else if (escaped == 'r') result.push_back('\r');
        else if (escaped == 't') result.push_back('\t');
        else if (escaped == 'u') {
            if (index + 4 > source.size()) return std::nullopt;
            unsigned code = 0;
            for (int digit = 0; digit < 4; ++digit) {
                const char hex = source[index++];
                code <<= 4;
                if (hex >= '0' && hex <= '9') code += hex - '0';
                else if (hex >= 'a' && hex <= 'f') code += hex - 'a' + 10;
                else if (hex >= 'A' && hex <= 'F') code += hex - 'A' + 10;
                else return std::nullopt;
            }
            if (code <= 0x7f) result.push_back(static_cast<char>(code));
            else if (code <= 0x7ff) {
                result.push_back(static_cast<char>(0xc0 | (code >> 6)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            } else {
                result.push_back(static_cast<char>(0xe0 | (code >> 12)));
                result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
        } else return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::size_t> FieldValue(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t position = 0;
    while ((position = json.find(needle, position)) != std::string_view::npos) {
        position = SkipSpace(json, position + needle.size());
        if (position < json.size() && json[position] == ':')
            return SkipSpace(json, position + 1);
    }
    return std::nullopt;
}

std::optional<std::string> StringField(std::string_view json, std::string_view key) {
    const auto value = FieldValue(json, key);
    if (!value) return std::nullopt;
    std::size_t position = *value;
    return ParseString(json, &position);
}

std::optional<long> LongField(std::string_view json, std::string_view key) {
    const auto value = FieldValue(json, key);
    if (!value) return std::nullopt;
    const char* begin = json.data() + *value;
    const char* end = begin;
    while (end < json.data() + json.size() && (*end == '-' || std::isdigit(static_cast<unsigned char>(*end)))) ++end;
    long parsed = 0;
    const auto result = std::from_chars(begin, end, parsed);
    return result.ec == std::errc{} && result.ptr == end ? std::optional<long>(parsed) : std::nullopt;
}

std::optional<std::uint64_t> SizeField(std::string_view json) {
    const auto text = StringField(json, "size");
    if (!text) return std::uint64_t{0};
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(text->data(), text->data() + text->size(), parsed);
    return result.ec == std::errc{} ? std::optional<std::uint64_t>(parsed) : std::nullopt;
}

std::optional<std::string_view> ArrayField(std::string_view json, std::string_view key) {
    const auto value = FieldValue(json, key);
    if (!value || *value >= json.size() || json[*value] != '[') return std::nullopt;
    bool quoted = false;
    bool escaped = false;
    int depth = 0;
    for (std::size_t index = *value; index < json.size(); ++index) {
        const char c = json[index];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
            continue;
        }
        if (c == '"') quoted = true;
        else if (c == '[') ++depth;
        else if (c == ']' && --depth == 0) return json.substr(*value + 1, index - *value - 1);
    }
    return std::nullopt;
}

std::vector<std::string_view> Objects(std::string_view array) {
    std::vector<std::string_view> result;
    bool quoted = false;
    bool escaped = false;
    int depth = 0;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < array.size(); ++index) {
        const char c = array[index];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
            continue;
        }
        if (c == '"') quoted = true;
        else if (c == '{') { if (depth++ == 0) begin = index; }
        else if (c == '}' && depth > 0 && --depth == 0) result.push_back(array.substr(begin, index - begin + 1));
    }
    return result;
}

std::optional<CloudSaveEntry> ParseFile(std::string_view object) {
    const auto id = StringField(object, "id");
    const auto name = StringField(object, "name");
    const auto modified = StringField(object, "modifiedTime");
    const auto size = SizeField(object);
    if (!id || !name || !modified || !size) return std::nullopt;
    return CloudSaveEntry{*id, *name, *modified, *size};
}

}  // namespace

std::optional<GoogleOAuthCredentials> parse_google_oauth_credentials(
    std::string_view json, std::string* error) {
    const auto installed = FieldValue(json, "installed");
    if (!installed || *installed >= json.size() || json[*installed] != '{') {
        if (error) *error = "Choose OAuth credentials created with application type Desktop app.";
        return std::nullopt;
    }
    const auto client_id = StringField(json, "client_id");
    const auto client_secret = StringField(json, "client_secret");
    if (!client_id || client_id->empty()) {
        if (error) *error = "The file does not contain a Google desktop OAuth client ID.";
        return std::nullopt;
    }
    if (error) error->clear();
    return GoogleOAuthCredentials{*client_id, client_secret.value_or("")};
}

std::optional<OAuthTokenResponse> parse_google_oauth_token(
    std::string_view json, std::string* error) {
    const auto access = StringField(json, "access_token");
    const auto expires = LongField(json, "expires_in");
    if (!access || access->empty() || !expires || *expires <= 0) {
        if (error) *error = google_api_error(json, "Google returned an invalid OAuth token response.");
        return std::nullopt;
    }
    if (error) error->clear();
    return OAuthTokenResponse{*access, StringField(json, "refresh_token").value_or(""), *expires};
}

std::optional<CloudSaveEntry> parse_google_drive_file(std::string_view json, std::string* error) {
    auto result = ParseFile(json);
    if (!result && error) *error = google_api_error(json, "Google Drive returned invalid file metadata.");
    else if (error) error->clear();
    return result;
}

std::optional<std::vector<CloudSaveEntry>> parse_google_drive_files(
    std::string_view json, std::string* error) {
    const auto files = ArrayField(json, "files");
    if (!files) {
        if (error) *error = google_api_error(json, "Google Drive returned an invalid backup list.");
        return std::nullopt;
    }
    std::vector<CloudSaveEntry> result;
    for (const auto object : Objects(*files)) {
        auto file = ParseFile(object);
        if (!file) {
            if (error) *error = "Google Drive returned incomplete backup metadata.";
            return std::nullopt;
        }
        if (file->name.rfind("say-count--", 0) == 0) result.push_back(std::move(*file));
    }
    if (error) error->clear();
    return result;
}

std::string google_api_error(std::string_view json, std::string_view fallback) {
    if (const auto description = StringField(json, "error_description"); description && !description->empty())
        return *description;
    if (const auto message = StringField(json, "message"); message && !message->empty()) return *message;
    if (const auto code = StringField(json, "error"); code && !code->empty()) return *code;
    return std::string(fallback);
}

std::string cloud_save_file_name(std::string_view project_name) {
    std::string slug;
    bool dash = false;
    for (const unsigned char c : project_name) {
        if (std::isalnum(c)) {
            slug.push_back(static_cast<char>(std::tolower(c)));
            dash = false;
        } else if (!slug.empty() && !dash) {
            slug.push_back('-');
            dash = true;
        }
        if (slug.size() >= 64) break;
    }
    while (!slug.empty() && slug.back() == '-') slug.pop_back();
    if (slug.empty()) slug = "untitled";
    return "say-count--" + slug + ".saycount.json";
}

std::string cloud_save_display_name(std::string_view file_name) {
    constexpr std::string_view prefix = "say-count--";
    constexpr std::string_view suffix = ".saycount.json";
    if (file_name.rfind(prefix, 0) != 0 || file_name.size() <= prefix.size() + suffix.size() ||
        file_name.substr(file_name.size() - suffix.size()) != suffix) return std::string(file_name);
    std::string value(file_name.substr(prefix.size(), file_name.size() - prefix.size() - suffix.size()));
    bool upper = true;
    for (char& c : value) {
        if (c == '-') { c = ' '; upper = true; }
        else if (upper) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); upper = false; }
    }
    return value;
}

}  // namespace say_count
