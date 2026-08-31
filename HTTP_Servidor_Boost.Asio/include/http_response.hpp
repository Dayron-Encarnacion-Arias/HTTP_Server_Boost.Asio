#pragma once

#include <map>
#include <string>
#include <string_view>

namespace http_server {

enum class HttpStatus : int {
    ok = 200,
    bad_request = 400,
    request_timeout = 408,
    not_found = 404,
    method_not_allowed = 405,
    payload_too_large = 413,
    internal_server_error = 500
};

class HttpResponse final {
public:
    explicit HttpResponse(HttpStatus status = HttpStatus::ok);

    HttpResponse& set_status(HttpStatus status) noexcept;
    HttpResponse& set_header(std::string name, std::string value);
    HttpResponse& set_body(std::string body);

    [[nodiscard]] HttpStatus status() const noexcept;
    [[nodiscard]] const std::string& body() const noexcept;
    [[nodiscard]] std::string header(std::string_view name) const;
    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] static std::string_view reason_phrase(HttpStatus status) noexcept;

private:
    HttpStatus status_;
    std::map<std::string, std::string, std::less<>> headers_;
    std::string body_;
};

} // namespace http_server

