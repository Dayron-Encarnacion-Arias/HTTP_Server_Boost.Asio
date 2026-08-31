#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace http_server {

void Logger::info(const std::string_view message) const { write("INFO", message); }
void Logger::error(const std::string_view message) const { write("ERROR", message); }

void Logger::write(const std::string_view level, const std::string_view message) const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &value);
#else
    localtime_r(&value, &local_time);
#endif

    const std::scoped_lock lock(mutex_);
    std::clog << '[' << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
              << "] [" << level << "] " << message << '\n';
}

} // namespace http_server
