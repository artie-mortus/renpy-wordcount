#pragma once

#include "core/cloud_save.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace say_count::app {

template <typename T>
struct GoogleDriveResult {
    T value{};
    std::string error;
    explicit operator bool() const { return error.empty(); }
};

class GoogleDriveClient final {
public:
    explicit GoogleDriveClient(std::string data_directory);

    bool configured() const { return credentials_.has_value(); }
    bool connected(std::string* error = nullptr) const;
    const std::string& configuration_path() const { return configuration_path_; }
    bool ImportClientConfiguration(std::string_view json, std::string* error = nullptr);

    GoogleDriveResult<bool> Connect(const std::function<bool(const std::string&)>& launch_browser,
                                    std::atomic_bool* cancelled = nullptr);
    GoogleDriveResult<bool> Disconnect();
    GoogleDriveResult<std::vector<CloudSaveEntry>> ListBackups();
    GoogleDriveResult<CloudSaveEntry> UploadBackup(
        std::string_view name, std::string_view contents,
        const std::optional<std::string>& existing_file_id = std::nullopt);
    GoogleDriveResult<std::string> DownloadBackup(std::string_view file_id);

private:
    struct HttpResponse {
        long status = 0;
        std::string body;
        std::string transport_error;
    };

    HttpResponse Request(std::string_view url, std::string_view method,
                         const std::vector<std::string>& headers = {},
                         std::string_view body = {}, std::size_t limit = 100 * 1024 * 1024) const;
    std::optional<std::string> EnsureAccessToken(std::string* error);
    std::optional<std::string> LoadRefreshToken(std::string* error = nullptr) const;
    bool SaveRefreshToken(std::string_view token, std::string* error);
    void LoadConfiguration();
    std::string FormValue(std::string_view value) const;
    std::vector<std::string> AuthorizationHeaders(const std::string& access_token,
                                                  std::string content_type = {}) const;

    std::string data_directory_;
    std::string configuration_path_;
    std::optional<GoogleOAuthCredentials> credentials_;
    std::string access_token_;
    std::chrono::steady_clock::time_point access_token_expiry_{};
};

}  // namespace say_count::app
