#include <catch2/catch_test_macros.hpp>

#include "core/cloud_save.h"

using namespace say_count;

TEST_CASE("Google desktop OAuth credentials parse from downloaded client JSON") {
    std::string error;
    const auto credentials = parse_google_oauth_credentials(R"({
      "installed":{"client_id":"123.apps.googleusercontent.com","client_secret":"secret"}
    })", &error);
    REQUIRE(credentials);
    CHECK(credentials->client_id == "123.apps.googleusercontent.com");
    CHECK(credentials->client_secret == "secret");
    CHECK(error.empty());

    CHECK_FALSE(parse_google_oauth_credentials(
        R"({"web":{"client_id":"wrong.apps.googleusercontent.com","client_secret":"secret"}})",
        &error));
    CHECK(error.find("Desktop app") != std::string::npos);
}

TEST_CASE("Google OAuth token parser accepts refreshed and initial tokens") {
    const auto initial = parse_google_oauth_token(
        R"({"access_token":"access","expires_in":3599,"refresh_token":"refresh"})");
    REQUIRE(initial);
    CHECK(initial->access_token == "access");
    CHECK(initial->refresh_token == "refresh");
    CHECK(initial->expires_in == 3599);

    const auto refreshed = parse_google_oauth_token(R"({"access_token":"next","expires_in":3600})");
    REQUIRE(refreshed);
    CHECK(refreshed->refresh_token.empty());
}

TEST_CASE("Drive app-data list keeps only Say Count backups") {
    std::string error;
    const auto files = parse_google_drive_files(R"({"files":[
      {"id":"a","name":"say-count--moonrise.saycount.json","modifiedTime":"2026-07-13T12:00:00Z","size":"2048"},
      {"id":"b","name":"other-app.json","modifiedTime":"2026-07-12T12:00:00Z","size":"10"}
    ]})", &error);
    REQUIRE(files);
    REQUIRE(files->size() == 1);
    CHECK((*files)[0].id == "a");
    CHECK((*files)[0].size == 2048);
    CHECK(error.empty());
}

TEST_CASE("Google API errors prefer useful server messages") {
    CHECK(google_api_error(R"({"error":{"code":403,"message":"Drive API is disabled"}})", "fallback") ==
          "Drive API is disabled");
    CHECK(google_api_error(R"({"error":"invalid_grant","error_description":"Token expired"})", "fallback") ==
          "Token expired");
}

TEST_CASE("cloud save names are stable and readable") {
    CHECK(cloud_save_file_name("My Ren'Py Story!") == "say-count--my-ren-py-story.saycount.json");
    CHECK(cloud_save_file_name("---") == "say-count--untitled.saycount.json");
    CHECK(cloud_save_display_name("say-count--my-ren-py-story.saycount.json") == "My Ren Py Story");
}
