#include "router.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace http_server {
namespace {

HttpRequest make_request(const std::string& method, const std::string& path) {
    const std::string raw = method + " " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest request;
    std::string error;
    if (HttpRequest::parse(raw, request, error) != ParseStatus::complete) {
        throw std::runtime_error(error);
    }
    return request;
}

class RouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() / "http_server_cpp_router_tests";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
        std::ofstream(root_ / "index.html") << "<h1>Home</h1>";
        std::ofstream(root_ / "404.html") << "<h1>Custom 404</h1>";
        std::ofstream(root_ / "styles.css") << "body{}";
        router_ = std::make_unique<Router>(root_, logger_);
    }

    void TearDown() override {
        router_.reset();
        std::filesystem::remove_all(root_);
    }

    Logger logger_;
    std::filesystem::path root_;
    std::unique_ptr<Router> router_;
};

TEST_F(RouterTest, FindsRoot) {
    const auto response = router_->route(make_request("GET", "/"));
    EXPECT_EQ(response.status(), HttpStatus::ok);
    EXPECT_EQ(response.body(), "<h1>Home</h1>");
}

TEST_F(RouterTest, FindsHealthEndpoint) {
    const auto response = router_->route(make_request("GET", "/api/health"));
    EXPECT_EQ(response.status(), HttpStatus::ok);
    EXPECT_NE(response.body().find("\"status\": \"ok\""), std::string::npos);
}

TEST_F(RouterTest, Returns404) {
    const auto response = router_->route(make_request("GET", "/missing"));
    EXPECT_EQ(response.status(), HttpStatus::not_found);
}

TEST_F(RouterTest, ServesRequiredContentType) {
    const auto response = router_->route(make_request("GET", "/styles.css"));
    EXPECT_EQ(response.header("Content-Type"), "text/css; charset=utf-8");
}

TEST_F(RouterTest, RejectsTraversal) {
    EXPECT_FALSE(Router::is_safe_path("/../../secret.txt"));
    EXPECT_FALSE(Router::is_safe_path("/%2e%2e/%2e%2e/secret.txt"));
    EXPECT_FALSE(Router::is_safe_path("/..%5csecret.txt"));
    EXPECT_EQ(router_->route(make_request("GET", "/%2e%2e/secret.txt")).status(),
              HttpStatus::bad_request);
}

TEST_F(RouterTest, RejectsUnsupportedMethod) {
    const auto response = router_->route(make_request("POST", "/"));
    EXPECT_EQ(response.status(), HttpStatus::method_not_allowed);
    EXPECT_EQ(response.header("Allow"), "GET");
}

TEST(ContentTypeTest, RecognizesRequiredTypes) {
    EXPECT_EQ(Router::content_type_for("a.html"), "text/html; charset=utf-8");
    EXPECT_EQ(Router::content_type_for("a.txt"), "text/plain; charset=utf-8");
    EXPECT_EQ(Router::content_type_for("a.json"), "application/json; charset=utf-8");
    EXPECT_EQ(Router::content_type_for("a.css"), "text/css; charset=utf-8");
    EXPECT_EQ(Router::content_type_for("a.js"), "application/javascript; charset=utf-8");
}

} // namespace
} // namespace http_server
