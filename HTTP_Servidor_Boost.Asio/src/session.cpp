#include "session.hpp"

#include "http_request.hpp"
#include "logger.hpp"
#include "router.hpp"

#include <boost/asio/bind_executor.hpp>

#include <exception>
#include <utility>

namespace http_server {

Session::Session(boost::asio::ip::tcp::socket socket,
                 const Router& router,
                 const Logger& logger)
    : socket_(std::move(socket)),
      strand_(boost::asio::make_strand(socket_.get_executor())),
      timer_(socket_.get_executor()),
      router_(router),
      logger_(logger) {
    request_buffer_.reserve(4096);
}

void Session::start() {
    boost::system::error_code error;
    const auto endpoint = socket_.remote_endpoint(error);
    const std::string client = error ? "unknown" :
        endpoint.address().to_string() + ':' + std::to_string(endpoint.port());
    logger_.info("Client connected: " + client);

    timer_.expires_after(timeout);
    const auto self = shared_from_this();
    timer_.async_wait(boost::asio::bind_executor(
        strand_, [self](const boost::system::error_code& timer_error) {
            if (!timer_error) {
                self->logger_.error("Request timeout");
                self->send_error(HttpStatus::request_timeout, "The request took too long");
            }
        }));
    read_request();
}

void Session::read_request() {
    const auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_),
        boost::asio::bind_executor(
            strand_, [self](const boost::system::error_code& error, const std::size_t bytes_read) {
                if (error) {
                    if (error != boost::asio::error::operation_aborted &&
                        error != boost::asio::error::eof &&
                        error != boost::asio::error::connection_reset) {
                        self->logger_.error("Connection read error: " + error.message());
                    }
                    self->close();
                    return;
                }
                if (self->request_buffer_.size() + bytes_read > max_request_size) {
                    self->send_error(HttpStatus::payload_too_large,
                                     "The request exceeds the 16 KiB limit");
                    return;
                }
                self->request_buffer_.append(self->read_buffer_.data(), bytes_read);
                self->process_request();
            }));
}

void Session::process_request() {
    HttpRequest request;
    std::string parse_error;
    const ParseStatus status = HttpRequest::parse(request_buffer_, request, parse_error);
    if (status == ParseStatus::incomplete) {
        read_request();
        return;
    }
    if (status == ParseStatus::invalid) {
        logger_.error("Invalid HTTP request: " + parse_error);
        send_error(HttpStatus::bad_request, parse_error);
        return;
    }

    try {
        send_response(router_.route(request).serialize());
    } catch (const std::exception& exception) {
        logger_.error("Request processing error: " + std::string(exception.what()));
        send_error(HttpStatus::internal_server_error, "The server could not process the request");
    }
}

void Session::send_response(std::string response) {
    if (response_started_) return;
    response_started_ = true;
    timer_.cancel();
    response_buffer_ = std::make_shared<std::string>(std::move(response));

    const auto self = shared_from_this();
    boost::asio::async_write(
        socket_, boost::asio::buffer(*response_buffer_),
        boost::asio::bind_executor(
            strand_, [self](const boost::system::error_code& error, std::size_t) {
                if (error && error != boost::asio::error::operation_aborted &&
                    error != boost::asio::error::connection_reset) {
                    self->logger_.error("Connection write error: " + error.message());
                }
                self->close();
            }));
}

void Session::send_error(const HttpStatus status, const std::string_view message) {
    HttpResponse response(status);
    response.set_header("Content-Type", "text/plain; charset=utf-8")
        .set_header("X-Content-Type-Options", "nosniff")
        .set_body(std::to_string(static_cast<int>(status)) + " " +
                  std::string(HttpResponse::reason_phrase(status)) + "\n" +
                  std::string(message) + "\n");
    send_response(response.serialize());
}

void Session::close() {
    timer_.cancel();
    boost::system::error_code ignored;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
}

} // namespace http_server
