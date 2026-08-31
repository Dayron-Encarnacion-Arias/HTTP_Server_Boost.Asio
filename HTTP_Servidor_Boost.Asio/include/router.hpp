#pragma once

#include "http_request.hpp"
#include "http_response.hpp"
#include "logger.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace http_server {

class Router final {
public:
    Router(std::filesystem::path public_root, const Logger& logger);

    [[nodiscard]] HttpResponse route(const HttpRequest& request) const;
    [[nodiscard]] static std::string content_type_for(const std::filesystem::path& path);
    [[nodiscard]] static bool is_safe_path(std::string_view request_path);

private:
    [[nodiscard]] HttpResponse serve_static(std::string_view request_path) const;
    [[nodiscard]] HttpResponse make_error(HttpStatus status, std::string_view message) const;
    [[nodiscard]] static std::string decode_path(std::string_view path, bool& valid);
    [[nodiscard]] static bool is_within_root(const std::filesystem::path& root,
                                             const std::filesystem::path& candidate);

    std::filesystem::path public_root_;
    const Logger& logger_;
};

} // namespace http_server

