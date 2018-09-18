#pragma once

#include "definitions.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

boost::asio::ip::tcp::endpoint make_endpoint(
    const std::string& host, std::uint16_t port);

using http_req = boost::beast::http::request<boost::beast::http::string_body>;
using http_res = boost::beast::http::response<boost::beast::http::string_body>;

using http_handler = std::function<void(http_req&, http_res&)>;

class http_client
{
public:
    http_client(boost::asio::io_context& ioc, boost::asio::ip::tcp::endpoint ep,
        std::chrono::seconds timeout = default_timeout);
    http_client(const http_client&) = delete;
    http_client& operator=(const http_client&) = delete;
    http_client(http_client&&) = delete;
    http_client& operator=(http_client&&) = delete;
    void operator()(http_req& req, http_res& res);
private:
    void connect(boost::asio::ip::tcp::endpoint ep);
    void send(http_req& req);
    void recv(http_res& res);
    void start_deadline();
    void check_deadline();
    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::system_timer deadline_;
    boost::beast::flat_buffer buffer_;
    std::chrono::seconds timeout_;
};

class http_server
{
public:
    http_server(boost::asio::io_context& ioc, boost::asio::ip::tcp::endpoint ep,
        http_handler handler,
        std::chrono::seconds timeout = default_timeout);
    http_server(const http_server&) = delete;
    http_server& operator=(const http_server&) = delete;
    http_server(http_server&&) = delete;
    http_server& operator=(http_server&&) = delete;
    void cancel();
    void operator()();
private:
    void accept();
    void send(http_res& res);
    void recv(http_req& req);
    void start_deadline();
    void check_deadline();
    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::system_timer deadline_;
    boost::beast::flat_buffer buffer_;
    std::chrono::seconds timeout_;
    http_handler handler_;
};
