#include "app/google_drive.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <wx/secretstore.h>

namespace say_count::app {
namespace {

constexpr const char* kSecretService = "Say Count Google Drive";
constexpr const char* kTokenEndpoint = "https://oauth2.googleapis.com/token";
constexpr const char* kDriveFiles = "https://www.googleapis.com/drive/v3/files";
constexpr const char* kDriveUpload = "https://www.googleapis.com/upload/drive/v3/files";

struct CurlRuntime {
    CurlRuntime() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlRuntime() { curl_global_cleanup(); }
};

CurlRuntime& Runtime() {
    static CurlRuntime runtime;
    return runtime;
}

std::optional<std::string> ReadFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::ostringstream output;
    output << input.rdbuf();
    return input.good() || input.eof() ? std::optional<std::string>(output.str()) : std::nullopt;
}

std::string Base64Url(const unsigned char* data, std::size_t size) {
    std::string encoded(4 * ((size + 2) / 3), '\0');
    const int length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()), data,
                                       static_cast<int>(size));
    encoded.resize(static_cast<std::size_t>(length));
    while (!encoded.empty() && encoded.back() == '=') encoded.pop_back();
    for (char& c : encoded) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    return encoded;
}

std::optional<std::string> RandomUrlToken(std::size_t bytes) {
    std::vector<unsigned char> random(bytes);
    if (RAND_bytes(random.data(), static_cast<int>(random.size())) != 1) return std::nullopt;
    return Base64Url(random.data(), random.size());
}

std::string PkceChallenge(std::string_view verifier) {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(reinterpret_cast<const unsigned char*>(verifier.data()), verifier.size(), digest.data());
    return Base64Url(digest.data(), digest.size());
}

std::string PercentDecode(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+' ) { result.push_back(' '); continue; }
        if (value[index] != '%' || index + 2 >= value.size()) { result.push_back(value[index]); continue; }
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int high = hex(value[index + 1]);
        const int low = hex(value[index + 2]);
        if (high < 0 || low < 0) { result.push_back(value[index]); continue; }
        result.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return result;
}

std::optional<std::string> QueryValue(std::string_view target, std::string_view key) {
    const auto question = target.find('?');
    if (question == std::string_view::npos) return std::nullopt;
    std::string_view query = target.substr(question + 1);
    while (!query.empty()) {
        const auto ampersand = query.find('&');
        const auto part = query.substr(0, ampersand);
        const auto equals = part.find('=');
        if (PercentDecode(part.substr(0, equals)) == key)
            return PercentDecode(equals == std::string_view::npos ? std::string_view{} : part.substr(equals + 1));
        if (ampersand == std::string_view::npos) break;
        query.remove_prefix(ampersand + 1);
    }
    return std::nullopt;
}

class LoopbackListener final {
public:
    LoopbackListener() {
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) return;
        const int enabled = 1;
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            listen(socket_, 1) != 0) {
            close(socket_);
            socket_ = -1;
            return;
        }
        socklen_t length = sizeof(address);
        if (getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &length) == 0)
            port_ = ntohs(address.sin_port);
    }
    ~LoopbackListener() { if (socket_ >= 0) close(socket_); }
    bool valid() const { return socket_ >= 0 && port_ != 0; }
    unsigned short port() const { return port_; }

    std::optional<std::string> Wait(std::atomic_bool* cancelled, std::string* error) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
        while (std::chrono::steady_clock::now() < deadline) {
            if (cancelled && cancelled->load()) { if (error) *error = "Google Drive connection cancelled."; return std::nullopt; }
            pollfd descriptor{socket_, POLLIN, 0};
            const int ready = poll(&descriptor, 1, 250);
            if (ready < 0 && errno != EINTR) { if (error) *error = "Could not receive Google's sign-in response."; return std::nullopt; }
            if (ready <= 0 || !(descriptor.revents & POLLIN)) continue;
            const int client = accept(socket_, nullptr, nullptr);
            if (client < 0) continue;
            std::array<char, 16384> request{};
            const ssize_t length = recv(client, request.data(), request.size() - 1, 0);
            std::string target;
            if (length > 0) {
                const std::string_view line(request.data(), static_cast<std::size_t>(length));
                const auto first_space = line.find(' ');
                const auto second_space = first_space == std::string_view::npos
                    ? std::string_view::npos : line.find(' ', first_space + 1);
                if (first_space != std::string_view::npos && second_space != std::string_view::npos)
                    target = std::string(line.substr(first_space + 1, second_space - first_space - 1));
            }
            const bool success = QueryValue(target, "code").has_value();
            const std::string body = success
                ? "<!doctype html><meta charset=utf-8><title>Say Count connected</title>"
                  "<body style='font:16px system-ui;margin:4rem;color:#17243a'>"
                  "<h1>Google Drive connected</h1><p>Return to Say Count. You can close this tab.</p></body>"
                : "<!doctype html><meta charset=utf-8><title>Say Count</title>"
                  "<body style='font:16px system-ui;margin:4rem;color:#17243a'>"
                  "<h1>Connection not completed</h1><p>Return to Say Count to try again.</p></body>";
            const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body;
            send(client, response.data(), response.size(), MSG_NOSIGNAL);
            close(client);
            if (target.empty()) { if (error) *error = "Google returned an invalid sign-in response."; return std::nullopt; }
            return target;
        }
        if (error) *error = "Google sign-in timed out.";
        return std::nullopt;
    }

private:
    int socket_ = -1;
    unsigned short port_ = 0;
};

struct WriteContext {
    std::string* body;
    std::size_t limit;
    bool exceeded = false;
};

std::size_t WriteBody(char* data, std::size_t size, std::size_t count, void* pointer) {
    auto* context = static_cast<WriteContext*>(pointer);
    const std::size_t bytes = size * count;
    if (bytes > context->limit - std::min(context->limit, context->body->size())) {
        context->exceeded = true;
        return 0;
    }
    context->body->append(data, bytes);
    return bytes;
}

}  // namespace

GoogleDriveClient::GoogleDriveClient(std::string data_directory)
    : data_directory_(std::move(data_directory)),
      configuration_path_((std::filesystem::path(data_directory_) / "google-oauth-client.json").string()) {
    Runtime();
    LoadConfiguration();
}

void GoogleDriveClient::LoadConfiguration() {
    const char* environment_id = std::getenv("SAY_COUNT_GOOGLE_CLIENT_ID");
    if (environment_id && *environment_id) {
        const char* environment_secret = std::getenv("SAY_COUNT_GOOGLE_CLIENT_SECRET");
        credentials_ = GoogleOAuthCredentials{environment_id, environment_secret ? environment_secret : ""};
        return;
    }
    const auto contents = ReadFile(configuration_path_);
    credentials_ = contents ? parse_google_oauth_credentials(*contents) : std::nullopt;
}

bool GoogleDriveClient::ImportClientConfiguration(std::string_view json, std::string* error) {
    auto parsed = parse_google_oauth_credentials(json, error);
    if (!parsed) return false;
    std::error_code ec;
    std::filesystem::create_directories(data_directory_, ec);
    if (ec) { if (error) *error = "Could not create the Say Count settings folder."; return false; }
    const std::string temporary = configuration_path_ + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
        if (!output) { if (error) *error = "Could not save the Google OAuth configuration."; return false; }
    }
    std::filesystem::permissions(temporary, std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
    std::filesystem::rename(temporary, configuration_path_, ec);
    if (ec) {
        std::filesystem::remove(temporary);
        if (error) *error = "Could not install the Google OAuth configuration.";
        return false;
    }
    credentials_ = std::move(parsed);
    access_token_.clear();
    if (error) error->clear();
    return true;
}

std::optional<std::string> GoogleDriveClient::LoadRefreshToken(std::string* error) const {
    if (!credentials_) { if (error) *error = "Google Drive is not configured."; return std::nullopt; }
#if !wxUSE_SECRETSTORE
    if (error) *error = "This build does not support secure system keyring storage.";
    return std::nullopt;
#else
    wxSecretStore store = wxSecretStore::GetDefault();
    wxString store_error;
    if (!store.IsOk(&store_error)) {
        if (error) *error = "The system keyring is unavailable: " + store_error.ToStdString();
        return std::nullopt;
    }
    wxString username;
    wxSecretValue secret;
    if (!store.Load(kSecretService, username, secret) ||
        username.ToStdString(wxConvUTF8) != credentials_->client_id || !secret.IsOk()) {
        if (error) error->clear();
        return std::nullopt;
    }
    wxString token = secret.GetAsString(wxConvUTF8);
    std::string result = token.ToStdString(wxConvUTF8);
    wxSecretValue::WipeString(token);
    if (error) error->clear();
    return result;
#endif
}

bool GoogleDriveClient::SaveRefreshToken(std::string_view token, std::string* error) {
#if !wxUSE_SECRETSTORE
    (void)token;
    if (error) *error = "This build does not support secure system keyring storage.";
    return false;
#else
    wxSecretStore store = wxSecretStore::GetDefault();
    wxString store_error;
    if (!store.IsOk(&store_error)) {
        if (error) *error = "The system keyring is unavailable: " + store_error.ToStdString();
        return false;
    }
    const wxSecretValue secret(wxString::FromUTF8(token.data(), token.size()));
    if (!store.Save(kSecretService, wxString::FromUTF8(credentials_->client_id), secret)) {
        if (error) *error = "Could not store the Google Drive sign-in in the system keyring.";
        return false;
    }
    if (error) error->clear();
    return true;
#endif
}

bool GoogleDriveClient::connected(std::string* error) const {
    return LoadRefreshToken(error).has_value();
}

std::string GoogleDriveClient::FormValue(std::string_view value) const {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    char* escaped = curl_easy_escape(curl, value.data(), static_cast<int>(value.size()));
    std::string result = escaped ? escaped : "";
    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);
    return result;
}

GoogleDriveClient::HttpResponse GoogleDriveClient::Request(
    std::string_view url, std::string_view method, const std::vector<std::string>& headers,
    std::string_view body, std::size_t limit) const {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    if (!curl) { response.transport_error = "Could not initialize secure networking."; return response; }
    curl_slist* raw_headers = nullptr;
    for (const auto& header : headers) raw_headers = curl_slist_append(raw_headers, header.c_str());
    WriteContext context{&response.body, limit};
    std::array<char, CURL_ERROR_SIZE> error_buffer{};
    const std::string url_copy(url);
    const std::string method_copy(method);
    curl_easy_setopt(curl, CURLOPT_URL, url_copy.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_copy.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, raw_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer.data());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SayCount/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (!body.empty() || method == "POST" || method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    }
    const CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    if (result != CURLE_OK) {
        response.transport_error = context.exceeded ? "Google Drive response exceeded the 100 MB safety limit." :
            (error_buffer[0] ? error_buffer.data() : curl_easy_strerror(result));
    }
    curl_slist_free_all(raw_headers);
    curl_easy_cleanup(curl);
    return response;
}

std::vector<std::string> GoogleDriveClient::AuthorizationHeaders(
    const std::string& access_token, std::string content_type) const {
    std::vector<std::string> headers{"Authorization: Bearer " + access_token};
    if (!content_type.empty()) headers.push_back("Content-Type: " + std::move(content_type));
    return headers;
}

GoogleDriveResult<bool> GoogleDriveClient::Connect(
    const std::function<bool(const std::string&)>& launch_browser, std::atomic_bool* cancelled) {
    if (!credentials_) return {{}, "Import a Google desktop OAuth client JSON file first."};
    LoopbackListener listener;
    if (!listener.valid()) return {{}, "Could not open the local sign-in callback."};
    const auto state = RandomUrlToken(24);
    const auto verifier = RandomUrlToken(48);
    if (!state || !verifier) return {{}, "Could not create secure sign-in parameters."};
    const std::string redirect = "http://127.0.0.1:" + std::to_string(listener.port());
    const std::string authorization = "https://accounts.google.com/o/oauth2/v2/auth?"
        "client_id=" + FormValue(credentials_->client_id) +
        "&redirect_uri=" + FormValue(redirect) +
        "&response_type=code&scope=" + FormValue("https://www.googleapis.com/auth/drive.appdata") +
        "&access_type=offline&prompt=consent&state=" + FormValue(*state) +
        "&code_challenge=" + FormValue(PkceChallenge(*verifier)) + "&code_challenge_method=S256";
    if (!launch_browser(authorization)) return {{}, "Could not open Google sign-in in the browser."};
    std::string wait_error;
    const auto target = listener.Wait(cancelled, &wait_error);
    if (!target) return {{}, wait_error};
    const auto returned_state = QueryValue(*target, "state");
    if (!returned_state || *returned_state != *state) return {{}, "Google sign-in state validation failed."};
    if (const auto oauth_error = QueryValue(*target, "error"))
        return {{}, *oauth_error == "access_denied" ? "Google Drive access was declined." : *oauth_error};
    const auto code = QueryValue(*target, "code");
    if (!code || code->empty()) return {{}, "Google did not return an authorization code."};
    std::string form = "code=" + FormValue(*code) + "&client_id=" + FormValue(credentials_->client_id) +
        "&redirect_uri=" + FormValue(redirect) + "&grant_type=authorization_code&code_verifier=" + FormValue(*verifier);
    if (!credentials_->client_secret.empty()) form += "&client_secret=" + FormValue(credentials_->client_secret);
    const auto response = Request(kTokenEndpoint, "POST", {"Content-Type: application/x-www-form-urlencoded"}, form);
    if (!response.transport_error.empty()) return {{}, response.transport_error};
    std::string parse_error;
    auto token = parse_google_oauth_token(response.body, &parse_error);
    if (response.status < 200 || response.status >= 300 || !token) return {{}, parse_error};
    if (token->refresh_token.empty()) return {{}, "Google did not issue an offline refresh token. Revoke access and try again."};
    if (!SaveRefreshToken(token->refresh_token, &parse_error)) return {{}, parse_error};
    access_token_ = std::move(token->access_token);
    access_token_expiry_ = std::chrono::steady_clock::now() +
        std::chrono::seconds(token->expires_in > 60 ? token->expires_in - 60 : 1);
    return {true, {}};
}

std::optional<std::string> GoogleDriveClient::EnsureAccessToken(std::string* error) {
    if (!access_token_.empty() && std::chrono::steady_clock::now() < access_token_expiry_) return access_token_;
    const auto refresh = LoadRefreshToken(error);
    if (!refresh) {
        if (error && error->empty()) *error = "Connect Google Drive first.";
        return std::nullopt;
    }
    std::string form = "client_id=" + FormValue(credentials_->client_id) +
        "&refresh_token=" + FormValue(*refresh) + "&grant_type=refresh_token";
    if (!credentials_->client_secret.empty()) form += "&client_secret=" + FormValue(credentials_->client_secret);
    const auto response = Request(kTokenEndpoint, "POST", {"Content-Type: application/x-www-form-urlencoded"}, form);
    if (!response.transport_error.empty()) { if (error) *error = response.transport_error; return std::nullopt; }
    auto token = parse_google_oauth_token(response.body, error);
    if (response.status < 200 || response.status >= 300 || !token) return std::nullopt;
    access_token_ = std::move(token->access_token);
    access_token_expiry_ = std::chrono::steady_clock::now() +
        std::chrono::seconds(token->expires_in > 60 ? token->expires_in - 60 : 1);
    return access_token_;
}

GoogleDriveResult<std::vector<CloudSaveEntry>> GoogleDriveClient::ListBackups() {
    std::string error;
    const auto access = EnsureAccessToken(&error);
    if (!access) return {{}, error};
    const auto response = Request(std::string(kDriveFiles) +
        "?spaces=appDataFolder&pageSize=100&orderBy=modifiedTime%20desc&fields=files(id%2Cname%2CmodifiedTime%2Csize)",
        "GET", AuthorizationHeaders(*access));
    if (!response.transport_error.empty()) return {{}, response.transport_error};
    auto files = parse_google_drive_files(response.body, &error);
    if (response.status < 200 || response.status >= 300 || !files) return {{}, error};
    return {std::move(*files), {}};
}

GoogleDriveResult<CloudSaveEntry> GoogleDriveClient::UploadBackup(
    std::string_view name, std::string_view contents, const std::optional<std::string>& existing_file_id) {
    std::string error;
    const auto access = EnsureAccessToken(&error);
    if (!access) return {{}, error};
    HttpResponse response;
    if (existing_file_id && !existing_file_id->empty()) {
        response = Request(std::string(kDriveUpload) + "/" + FormValue(*existing_file_id) +
            "?uploadType=media&fields=id%2Cname%2CmodifiedTime%2Csize", "PATCH",
            AuthorizationHeaders(*access, "application/vnd.say-count.project+json"), contents);
    } else {
        const std::string boundary = "say_count_" + RandomUrlToken(18).value_or("upload");
        const std::string metadata = "{\"name\":\"" + std::string(name) +
            "\",\"parents\":[\"appDataFolder\"]}";
        const std::string body = "--" + boundary + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n" +
            metadata + "\r\n--" + boundary +
            "\r\nContent-Type: application/vnd.say-count.project+json\r\n\r\n" +
            std::string(contents) + "\r\n--" + boundary + "--\r\n";
        response = Request(std::string(kDriveUpload) +
            "?uploadType=multipart&fields=id%2Cname%2CmodifiedTime%2Csize", "POST",
            AuthorizationHeaders(*access, "multipart/related; boundary=" + boundary), body);
    }
    if (!response.transport_error.empty()) return {{}, response.transport_error};
    auto file = parse_google_drive_file(response.body, &error);
    if (response.status < 200 || response.status >= 300 || !file) return {{}, error};
    return {std::move(*file), {}};
}

GoogleDriveResult<std::string> GoogleDriveClient::DownloadBackup(std::string_view file_id) {
    std::string error;
    const auto access = EnsureAccessToken(&error);
    if (!access) return {{}, error};
    const auto response = Request(std::string(kDriveFiles) + "/" + FormValue(file_id) + "?alt=media",
                                  "GET", AuthorizationHeaders(*access));
    if (!response.transport_error.empty()) return {{}, response.transport_error};
    if (response.status < 200 || response.status >= 300)
        return {{}, google_api_error(response.body, "Could not download the Google Drive backup.")};
    return {response.body, {}};
}

GoogleDriveResult<bool> GoogleDriveClient::Disconnect() {
    std::string error;
    const auto refresh = LoadRefreshToken(&error);
    if (!refresh && !error.empty()) return {{}, error};
    if (refresh) {
        Request("https://oauth2.googleapis.com/revoke", "POST",
            {"Content-Type: application/x-www-form-urlencoded"}, "token=" + FormValue(*refresh));
    }
#if wxUSE_SECRETSTORE
    if (refresh) wxSecretStore::GetDefault().Delete(kSecretService);
#endif
    access_token_.clear();
    access_token_expiry_ = {};
    return {true, {}};
}

}  // namespace say_count::app
