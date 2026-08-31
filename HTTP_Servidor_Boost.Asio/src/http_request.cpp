#include "http_request.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>
#include <utility>

namespace http_server {
namespace {

std::string trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

bool is_token(const std::string_view value) {
    constexpr std::string_view separators{"()<>@,;:\\\"/[]?={} \t"};
    return !value.empty() && value.find_first_of(separators) == std::string_view::npos &&
           std::ranges::all_of(value, [](const unsigned char character) {
               return character > 31U && character < 127U;
           });
}

} // namespace

ParseStatus HttpRequest::parse(const std::string_view raw,
                               HttpRequest& output,
                               std::string& error_message) {
    error_message.clear();
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        if (raw.find("\n\n") != std::string_view::npos) {
            error_message = "HTTP headers must use CRLF line endings";
            return ParseStatus::invalid;
        }
        return ParseStatus::incomplete;
    }

    const auto request_line_end = raw.find("\r\n");
    if (request_line_end == std::string_view::npos || request_line_end == 0 ||
        request_line_end > header_end) {
        error_message = "Missing or invalid request line";
        return ParseStatus::invalid;
    }

    HttpRequest parsed;
    {
        std::istringstream stream(std::string(raw.substr(0, request_line_end)));
        std::string trailing;
        if (!(stream >> parsed.method_ >> parsed.target_ >> parsed.version_) || (stream >> trailing)) {
            error_message = "Malformed request line";
            return ParseStatus::invalid;
        }
    }

    if (!is_token(parsed.method_)) {
        error_message = "Invalid HTTP method";
        return ParseStatus::invalid;
    }
    if (parsed.target_.empty() || parsed.target_.front() != '/' ||
        parsed.target_.find('#') != std::string::npos) {
        error_message = "Invalid request target";
        return ParseStatus::invalid;
    }
    if (parsed.version_ != "HTTP/1.1" && parsed.version_ != "HTTP/1.0") {
        error_message = "Unsupported HTTP version";
        return ParseStatus::invalid;
    }
    parsed.path_ = parsed.target_.substr(0, parsed.target_.find('?'));

    std::size_t cursor = request_line_end + 2;
    while (cursor < header_end) {
        const auto line_end = raw.find("\r\n", cursor);
        if (line_end == std::string_view::npos || line_end > header_end) {
            error_message = "Malformed header line";
            return ParseStatus::invalid;
        }
        const auto line = raw.substr(cursor, line_end - cursor);
        if (line.empty() || line.front() == ' ' || line.front() == '\t') {
            error_message = "Obsolete or empty header line";
            return ParseStatus::invalid;
        }
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || !is_token(line.substr(0, colon))) {
            error_message = "Invalid header";
            return ParseStatus::invalid;
        }

        const auto name = lowercase(line.substr(0, colon));
        if (parsed.headers_.contains(name)) {
            error_message = "Duplicate headers are not supported";
            return ParseStatus::invalid;
        }
        parsed.headers_.emplace(name, trim(line.substr(colon + 1)));
        cursor = line_end + 2;
    }

    if (parsed.version_ == "HTTP/1.1" && !parsed.headers_.contains("host")) {
        error_message = "HTTP/1.1 requires a Host header";
        return ParseStatus::invalid;
    }
    if (parsed.headers_.contains("transfer-encoding")) {
        error_message = "Transfer-Encoding is not supported";
        return ParseStatus::invalid;
    }

    std::size_t content_length = 0;
    if (const auto iterator = parsed.headers_.find("content-length");
        iterator != parsed.headers_.end()) {
        const auto& value = iterator->second;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), content_length);
        if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
            error_message = "Invalid Content-Length";
            return ParseStatus::invalid;
        }
    }

    const std::size_t body_start = header_end + 4;
    if (content_length > std::numeric_limits<std::size_t>::max() - body_start) {
        error_message = "Content-Length is too large";
        return ParseStatus::invalid;
    }
    if (raw.size() < body_start + content_length) {
        return ParseStatus::incomplete;
    }

    parsed.body_.assign(raw.substr(body_start, content_length));
    output = std::move(parsed);
    return ParseStatus::complete;
}

const std::string& HttpRequest::method() const noexcept { return method_; }
const std::string& HttpRequest::target() const noexcept { return target_; }
const std::string& HttpRequest::path() const noexcept { return path_; }
const std::string& HttpRequest::version() const noexcept { return version_; }
const HttpRequest::Headers& HttpRequest::headers() const noexcept { return headers_; }
const std::string& HttpRequest::body() const noexcept { return body_; }

std::string HttpRequest::header(const std::string_view name) const {
    const auto iterator = headers_.find(lowercase(name));
    return iterator == headers_.end() ? std::string{} : iterator->second;
}

} // namespace http_server
