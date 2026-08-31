#pragma once

#include "http_response.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace http_server {

class Logger;
class Router;

class Session final : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket,
            const Router& router,
            const Logger& logger);

    void start();

private:
    static constexpr std::size_t max_request_size = 16U * 1024U;
    static constexpr auto timeout = std::chrono::seconds(10);

    void read_request();
    void process_request();
    void send_response(std::string response);
    void send_error(HttpStatus status, std::string_view message);
    void close();

    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    boost::asio::steady_timer timer_;
    const Router& router_;
    const Logger& logger_;
    std::array<char, 4096> read_buffer_{};
    std::string request_buffer_;
    std::shared_ptr<std::string> response_buffer_;
    bool response_started_{false};
};

} // namespace http_server

