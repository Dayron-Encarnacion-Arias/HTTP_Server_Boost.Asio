#include "http_response.hpp"

#include <gtest/gtest.h>

#include <string>

namespace http_server {
namespace {

TEST(HttpResponseTest, SerializesStatusHeadersAndBody) {
    HttpResponse response(HttpStatus::ok);
    response.set_header("Content-Type", "application/json").set_body("{\"ok\":true}");
    const std::string serialized = response.serialize();
    EXPECT_TRUE(serialized.starts_with("HTTP/1.1 200 OK\r\n"));
    EXPECT_NE(serialized.find("Content-Type: application/json\r\n"), std::string::npos);
    EXPECT_NE(serialized.find("Content-Length: 11\r\n"), std::string::npos);
    EXPECT_TRUE(serialized.ends_with("{\"ok\":true}"));
}

TEST(HttpResponseTest, SerializesNotFound) {
    HttpResponse response(HttpStatus::not_found);
    response.set_body("missing");
    EXPECT_TRUE(response.serialize().starts_with("HTTP/1.1 404 Not Found\r\n"));
}

TEST(HttpResponseTest, AlwaysClosesConnection) {
    EXPECT_NE(HttpResponse().serialize().find("Connection: close\r\n"), std::string::npos);
}

} // namespace
} // namespace http_server
