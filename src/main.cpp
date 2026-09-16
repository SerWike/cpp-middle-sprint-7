#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <print>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::transfer_exactly;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";

struct SocketGuard {
    tcp::socket &_sock;

    SocketGuard(tcp::socket &sock) : _sock(sock) {};

    ~SocketGuard() {
        boost::system::error_code ec;
        if (_sock.is_open()) {
            _sock.shutdown(tcp::socket::shutdown_both, ec);
            _sock.close(ec);
        }
    }
};

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {

    auto buffer_to_string = [](boost::asio::streambuf &buffer, unsigned long buffer_size, std::string &str) {
        if (buffer.size() > 0) {
            str.clear();
            str.resize(buffer_size);
            buffer.sgetn(str.data(), buffer_size);
        }
    };

    SocketGuard client(client_socket);

    boost::asio::streambuf buf;
    unsigned long bytes_ready, sended_bytes, total_size;
    size_t headers_size = 0;

    std::string req, resp;
    std::optional<size_t> content_length;
    try {
        bytes_ready = co_await async_read_until(client._sock, buf, delimiter, use_awaitable);
        buffer_to_string(buf, bytes_ready, req);

        auto [host, port] = findHostPort(req);
        if (host.empty()) {
            std::println("Invalid request without 'Host'; session closed");
            co_return;
        }

        tcp::resolver resolver(io_service);
        auto endpoints = co_await resolver.async_resolve(host, port, use_awaitable);

        tcp::socket dest_socket(io_service);
        SocketGuard dest(dest_socket);

        co_await async_connect(dest._sock, endpoints, use_awaitable);
        int a;
        co_await async_write(dest._sock, buffer(req), use_awaitable);

        bytes_ready = co_await async_read_until(dest._sock, buf, delimiter, use_awaitable);
        buffer_to_string(buf, bytes_ready, resp);

        co_await async_write(client._sock, buffer(resp), use_awaitable);

        content_length = findContentLength(resp);

        if (content_length && content_length.value() > 0) {
            size_t already = buf.size();
            size_t need = content_length.value();
            std::string body;
            if (already > 0) {

                buffer_to_string(buf, already, body);
                co_await async_write(client._sock, buffer(body), use_awaitable);

                need -= already;
            }

            if (need > 0) {
                bytes_ready = co_await async_read(dest._sock, buf, transfer_exactly(need), use_awaitable);
                buffer_to_string(buf, bytes_ready, body);
                co_await async_write(client._sock, buffer(body), use_awaitable);
            }
        }

    } catch (const std::exception &err) {
        std::println("Error: {}", err.what());
    }

    co_return;
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (ec)
                std::cerr << "Error in accept: " << ec.message() << std::endl;
            else {
                co_spawn(this->io_service_, session(std::move(this->socket_), this->io_service_),
                         boost::asio::detached);

                do_accept();
            };
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
