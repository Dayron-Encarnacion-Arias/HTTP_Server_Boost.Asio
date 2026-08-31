#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace http_server {

enum class ParseStatus { complete, incomplete, invalid };

class HttpRequest final {
public:
    using Headers = std::unordered_map<std::string, std::string>;

    static ParseStatus parse(std::string_view raw,
                             HttpRequest& output,
                             std::string& error_message);

    [[nodiscard]] const std::string& method() const noexcept;
    [[nodiscard]] const std::string& target() const noexcept;
    [[nodiscard]] const std::string& path() const noexcept;
    [[nodiscard]] const std::string& version() const noexcept;
    [[nodiscard]] const Headers& headers() const noexcept;
    [[nodiscard]] const std::string& body() const noexcept;
    [[nodiscard]] std::string header(std::string_view name) const;

private:
    std::string method_;
    std::string target_;
    std::string path_;
    std::string version_;
    Headers headers_;
    std::string body_;
};

} // namespace http_server
