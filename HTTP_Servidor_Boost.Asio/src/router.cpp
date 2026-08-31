#include "router.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>

namespace http_server {
namespace {

int hex_value(const char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

std::string escape_html(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}

} // namespace

Router::Router(std::filesystem::path public_root, const Logger& logger)
    : public_root_(std::filesystem::absolute(std::move(public_root)).lexically_normal()),
      logger_(logger) {}

HttpResponse Router::route(const HttpRequest& request) const {
    logger_.info(request.method() + " " + request.path());

    if (request.method() != "GET") {
        auto response = make_error(HttpStatus::method_not_allowed, "Only GET is supported");
        response.set_header("Allow", "GET");
        return response;
    }
    if (request.path() == "/api/health") {
        HttpResponse response(HttpStatus::ok);
        response.set_header("Content-Type", "application/json; charset=utf-8")
            .set_header("Cache-Control", "no-store")
            .set_body("{\n  \"status\": \"ok\"\n}\n");
        return response;
    }
    if (request.path() == "/") return serve_static("/index.html");
    if (request.path() == "/about") return serve_static("/about.html");
    return serve_static(request.path());
}

std::string Router::content_type_for(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "application/javascript; charset=utf-8";
    return "application/octet-stream";
}

bool Router::is_safe_path(const std::string_view request_path) {
    bool valid = false;
    const std::string decoded = decode_path(request_path, valid);
    if (!valid || decoded.empty() || decoded.front() != '/' ||
        decoded.find('\\') != std::string::npos || decoded.find(':') != std::string::npos ||
        decoded.find('\0') != std::string::npos) {
        return false;
    }
    const auto relative = std::filesystem::path(decoded).relative_path();
    return std::ranges::none_of(relative, [](const auto& component) { return component == ".."; });
}

HttpResponse Router::serve_static(const std::string_view request_path) const {
    if (!is_safe_path(request_path)) {
        logger_.error("Rejected unsafe path: " + std::string(request_path));
        return make_error(HttpStatus::bad_request, "The requested path is invalid");
    }

    bool valid = false;
    const std::string decoded = decode_path(request_path, valid);
    const auto relative = std::filesystem::path(decoded).relative_path().lexically_normal();
    const auto candidate = (public_root_ / relative).lexically_normal();

    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate, error)) {
        logger_.info("404 " + std::string(request_path));
        return make_error(HttpStatus::not_found, "The requested resource was not found");
    }

    const auto canonical_root = std::filesystem::weakly_canonical(public_root_, error);
    if (error) {
        logger_.error("Cannot resolve public root: " + error.message());
        return make_error(HttpStatus::internal_server_error, "Cannot read the public directory");
    }
    const auto canonical_candidate = std::filesystem::canonical(candidate, error);
    if (error || !is_within_root(canonical_root, canonical_candidate)) {
        logger_.error("Rejected path outside public root: " + candidate.string());
        return make_error(HttpStatus::bad_request, "The requested path is invalid");
    }

    std::ifstream file(canonical_candidate, std::ios::binary);
    if (!file) {
        logger_.error("Cannot open file: " + canonical_candidate.string());
        return make_error(HttpStatus::internal_server_error, "Cannot read the requested resource");
    }
    std::string body{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    HttpResponse response(HttpStatus::ok);
    response.set_header("Content-Type", content_type_for(canonical_candidate))
        .set_header("X-Content-Type-Options", "nosniff")
        .set_body(std::move(body));
    return response;
}

HttpResponse Router::make_error(const HttpStatus status, const std::string_view message) const {
    if (status == HttpStatus::not_found) {
        std::ifstream file(public_root_ / "404.html", std::ios::binary);
        if (file) {
            HttpResponse response(status);
            response.set_header("Content-Type", "text/html; charset=utf-8")
                .set_header("X-Content-Type-Options", "nosniff")
                .set_body(std::string(std::istreambuf_iterator<char>(file),
                                      std::istreambuf_iterator<char>()));
            return response;
        }
    }

    std::ostringstream body;
    body << "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\"><title>"
         << static_cast<int>(status) << ' ' << HttpResponse::reason_phrase(status)
         << "</title><style>body{background:#080808;color:#f5f5f5;font-family:system-ui;"
         << "display:grid;place-items:center;min-height:100vh;margin:0}main{border-left:4px solid #e10600;"
         << "padding:2rem}h1{color:#ff3028}</style></head><body><main><h1>"
         << static_cast<int>(status) << ' ' << HttpResponse::reason_phrase(status)
         << "</h1><p>" << escape_html(message) << "</p></main></body></html>";
    HttpResponse response(status);
    response.set_header("Content-Type", "text/html; charset=utf-8")
        .set_header("X-Content-Type-Options", "nosniff")
        .set_body(body.str());
    return response;
}

std::string Router::decode_path(const std::string_view path, bool& valid) {
    valid = false;
    std::string decoded;
    decoded.reserve(path.size());
    for (std::size_t index = 0; index < path.size(); ++index) {
        if (path[index] != '%') {
            decoded += path[index];
            continue;
        }
        if (index + 2 >= path.size()) return {};
        const int high = hex_value(path[index + 1]);
        const int low = hex_value(path[index + 2]);
        if (high < 0 || low < 0) return {};
        decoded += static_cast<char>((high << 4) | low);
        index += 2;
    }
    valid = true;
    return decoded;
}

bool Router::is_within_root(const std::filesystem::path& root,
                            const std::filesystem::path& candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end() && candidate_part != candidate.end(); ++root_part, ++candidate_part) {
        if (*root_part != *candidate_part) return false;
    }
    return root_part == root.end();
}

} // namespace http_server
