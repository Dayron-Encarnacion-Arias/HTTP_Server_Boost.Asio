#include "http_request.hpp"

#include <gtest/gtest.h>

#include <string>

namespace http_server {
namespace {

TEST(HttpRequestTest, ParsesValidGet) {
    constexpr std::string_view raw =
        "GET /about?from=test HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    HttpRequest request;
    std::string error;
    EXPECT_EQ(HttpRequest::parse(raw, request, error), ParseStatus::complete);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(request.method(), "GET");
    EXPECT_EQ(request.target(), "/about?from=test");
    EXPECT_EQ(request.path(), "/about");
    EXPECT_EQ(request.version(), "HTTP/1.1");
    EXPECT_EQ(request.header("HOST"), "localhost");
}

TEST(HttpRequestTest, ParsesBodyFromContentLength) {
    constexpr std::string_view raw =
        "POST /event HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello";
    HttpRequest request;
    std::string error;
    EXPECT_EQ(HttpRequest::parse(raw, request, error), ParseStatus::complete);
    EXPECT_EQ(request.body(), "hello");
}

TEST(HttpRequestTest, ReportsIncompleteBody) {
    constexpr std::string_view raw =
        "POST /event HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhi";
    HttpRequest request;
    std::string error;
    EXPECT_EQ(HttpRequest::parse(raw, request, error), ParseStatus::incomplete);
}

TEST(HttpRequestTest, RejectsMissingHost) {
    constexpr std::string_view raw = "GET / HTTP/1.1\r\nConnection: close\r\n\r\n";
    HttpRequest request;
    std::string error;
    EXPECT_EQ(HttpRequest::parse(raw, request, error), ParseStatus::invalid);
    EXPECT_FALSE(error.empty());
}

TEST(HttpRequestTest, RejectsMalformedHeader) {
    constexpr std::string_view raw = "GET / HTTP/1.1\r\nHost localhost\r\n\r\n";
    HttpRequest request;
    std::string error;
    EXPECT_EQ(HttpRequest::parse(raw, request, error), ParseStatus::invalid);
}

} // namespace
} // namespace http_server
