#include "logger.hpp"
#include "router.hpp"
#include "server.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <charconv>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::filesystem::path default_public_root() {
    const auto current = std::filesystem::current_path();
    const auto direct = current / "public";
    if (std::filesystem::is_directory(direct)) return direct;

    const auto from_solution = current / "HTTP_Servidor_Boost.Asio" / "public";
    if (std::filesystem::is_directory(from_solution)) return from_solution;
    return direct;
}

struct Configuration {
    std::string host{"0.0.0.0"};
    unsigned short port{8080};
    std::filesystem::path public_root{default_public_root()};
    bool show_help{false};
};

unsigned short parse_port(const std::string_view text) {
    unsigned int port = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), port);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        port == 0 || port > 65535) {
        throw std::invalid_argument("--port must be between 1 and 65535");
    }
    return static_cast<unsigned short>(port);
}

Configuration parse_arguments(const int argc, char* argv[]) {
    Configuration configuration;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            configuration.show_help = true;
        } else if (argument == "--host" && index + 1 < argc) {
            configuration.host = argv[++index];
        } else if (argument == "--port" && index + 1 < argc) {
            configuration.port = parse_port(argv[++index]);
        } else if (argument == "--public-root" && index + 1 < argc) {
            configuration.public_root = argv[++index];
        } else {
            throw std::invalid_argument("Unknown or incomplete argument: " + std::string(argument));
        }
    }
    return configuration;
}

void print_usage() {
    std::cout << "HTTP Server C++ / Boost.Asio\n\n"
              << "Usage: HTTP_Servidor_Boost.Asio.exe [--host ADDRESS] [--port PORT] "
                 "[--public-root PATH]\n"
              << "Defaults: --host 0.0.0.0 --port 8080 --public-root ./public\n";
}

} // namespace

int main(const int argc, char* argv[]) {
    http_server::Logger logger;
    try {
        const Configuration configuration = parse_arguments(argc, argv);
        if (configuration.show_help) {
            print_usage();
            return 0;
        }
        if (!std::filesystem::is_directory(configuration.public_root)) {
            throw std::runtime_error("Public directory not found: " +
                                     configuration.public_root.string());
        }

        boost::asio::io_context io_context;
        http_server::Router router(configuration.public_root, logger);
        http_server::Server server(io_context, configuration.host, configuration.port, router, logger);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &logger](const boost::system::error_code& error, int) {
            if (!error) {
                logger.info("Shutdown signal received");
                io_context.stop();
            }
        });

        server.start();
        const unsigned int thread_count = std::clamp(std::thread::hardware_concurrency(), 2U, 8U);
        logger.info("I/O worker threads: " + std::to_string(thread_count));

        std::vector<std::jthread> workers;
        workers.reserve(thread_count - 1);
        for (unsigned int index = 1; index < thread_count; ++index) {
            workers.emplace_back([&io_context] { io_context.run(); });
        }
        io_context.run();
        return 0;
    } catch (const std::exception& exception) {
        logger.error(exception.what());
        print_usage();
        return 1;
    }
}
