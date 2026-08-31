#pragma once

#include <boost/asio.hpp>

#include <string>

namespace http_server {

class Logger;
class Router;

class Server final {
public:
    Server(boost::asio::io_context& io_context,
           std::string host,
           unsigned short port,
           const Router& router,
           const Logger& logger);

    void start();

private:
    void accept_next();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::string host_;
    unsigned short port_;
    const Router& router_;
    const Logger& logger_;
};

} // namespace http_server
