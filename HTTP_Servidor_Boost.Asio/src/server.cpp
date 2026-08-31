#include "server.hpp"

#include "logger.hpp"
#include "router.hpp"
#include "session.hpp"

#include <memory>
#include <utility>

namespace http_server {

Server::Server(boost::asio::io_context& io_context,
               std::string host,
               const unsigned short port,
               const Router& router,
               const Logger& logger)
    : io_context_(io_context),
      acceptor_(io_context),
      host_(std::move(host)),
      port_(port),
      router_(router),
      logger_(logger) {
    const boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(host_), port_);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);
}

void Server::start() {
    logger_.info("Server started on " + host_ + ':' + std::to_string(port_));
    accept_next();
}

void Server::accept_next() {
    acceptor_.async_accept([this](const boost::system::error_code& error,
                                  boost::asio::ip::tcp::socket socket) {
        if (!error) {
            std::make_shared<Session>(std::move(socket), router_, logger_)->start();
        } else if (error != boost::asio::error::operation_aborted) {
            logger_.error("Accept error: " + error.message());
        }
        if (acceptor_.is_open()) accept_next();
    });
}

} // namespace http_server

