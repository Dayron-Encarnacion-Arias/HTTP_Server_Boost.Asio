#include "http_response.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace http_server {
namespace {

bool equals_ignore_case(const std::string_view left, const std::string_view right) {
    return left.size() == right.size() &&
           std::ranges::equal(left, right, [](const unsigned char a, const unsigned char b) {
               return std::tolower(a) == std::tolower(b);
           });
}

} // namespace

HttpResponse::HttpResponse(const HttpStatus status) : status_(status) {}

HttpResponse& HttpResponse::set_status(const HttpStatus status) noexcept {
    status_ = status;
    return *this;
}

HttpResponse& HttpResponse::set_header(std::string name, std::string value) {
    headers_.insert_or_assign(std::move(name), std::move(value));
    return *this;
}

HttpResponse& HttpResponse::set_body(std::string body) {
    body_ = std::move(body);
    return *this;
}

HttpStatus HttpResponse::status() const noexcept { return status_; }
const std::string& HttpResponse::body() const noexcept { return body_; }

std::string HttpResponse::header(const std::string_view name) const {
    const auto iterator = std::ranges::find_if(headers_, [name](const auto& item) {
        return equals_ignore_case(item.first, name);
    });
    return iterator == headers_.end() ? std::string{} : iterator->second;
}

std::string HttpResponse::serialize() const {
    std::ostringstream response;
    response << "HTTP/1.1 " << static_cast<int>(status_) << ' ' << reason_phrase(status_) << "\r\n";
    bool content_type_exists = false;
    for (const auto& [name, value] : headers_) {
        if (equals_ignore_case(name, "Content-Length") || equals_ignore_case(name, "Connection")) {
            continue;
        }
        content_type_exists = content_type_exists || equals_ignore_case(name, "Content-Type");
        response << name << ": " << value << "\r\n";
    }
    if (!content_type_exists) {
        response << "Content-Type: text/plain; charset=utf-8\r\n";
    }
    response << "Content-Length: " << body_.size() << "\r\n"
             << "Connection: close\r\n\r\n" << body_;
    return response.str();
}

std::string_view HttpResponse::reason_phrase(const HttpStatus status) noexcept {
    switch (status) {
    case HttpStatus::ok: return "OK";
    case HttpStatus::bad_request: return "Bad Request";
    case HttpStatus::request_timeout: return "Request Timeout";
    case HttpStatus::not_found: return "Not Found";
    case HttpStatus::method_not_allowed: return "Method Not Allowed";
    case HttpStatus::payload_too_large: return "Payload Too Large";
    case HttpStatus::internal_server_error: return "Internal Server Error";
    }
    return "Unknown";
}

} // namespace http_server
