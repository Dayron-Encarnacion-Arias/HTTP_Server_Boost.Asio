#pragma once

#include <mutex>
#include <string_view>

namespace http_server {

class Logger final {
public:
    void info(std::string_view message) const;
    void error(std::string_view message) const;

private:
    void write(std::string_view level, std::string_view message) const;
    mutable std::mutex mutex_;
};

} // namespace http_server
