#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct GoogleOAuthCredentials {
    std::string client_id;
    std::string client_secret;
};

struct OAuthTokenResponse {
    std::string access_token;
    std::string refresh_token;
    long expires_in = 0;
};

struct CloudSaveEntry {
    std::string id;
    std::string name;
    std::string modified_time;
    std::uint64_t size = 0;
};

std::optional<GoogleOAuthCredentials> parse_google_oauth_credentials(
    std::string_view json, std::string* error = nullptr);
std::optional<OAuthTokenResponse> parse_google_oauth_token(
    std::string_view json, std::string* error = nullptr);
std::optional<CloudSaveEntry> parse_google_drive_file(
    std::string_view json, std::string* error = nullptr);
std::optional<std::vector<CloudSaveEntry>> parse_google_drive_files(
    std::string_view json, std::string* error = nullptr);
std::string google_api_error(std::string_view json, std::string_view fallback);

std::string cloud_save_file_name(std::string_view project_name);
std::string cloud_save_display_name(std::string_view file_name);

}  // namespace say_count
