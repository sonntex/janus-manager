#include "http.hpp"

boost::asio::ip::tcp::endpoint make_endpoint(const std::string& host, std::uint16_t port)
{ return {boost::asio::ip::make_address(host), port}; }

http_client::http_client(boost::asio::io_context& ioc, boost::asio::ip::tcp::endpoint ep,
    std::chrono::seconds timeout)
    : ioc_(ioc), socket_(ioc), deadline_(ioc), timeout_(timeout)
{
    start_deadline();
    connect(ep);
}

void http_client::connect(boost::asio::ip::tcp::endpoint ep)
{
    boost::system::error_code ec = boost::asio::error::would_block;
    deadline_.expires_from_now(timeout_);
    socket_.async_connect(ep,
        [&ec](boost::system::error_code e) { ec = e; });
    do { ioc_.run_one(); } while (ec == boost::asio::error::would_block);
    if (ec)
        throw boost::system::system_error(ec);
}

void http_client::send(http_req& req)
{
    boost::system::error_code ec = boost::asio::error::would_block;
    deadline_.expires_from_now(timeout_);
    boost::beast::http::async_write(socket_, req,
        [&ec](boost::system::error_code e, std::size_t) { ec = e; });
    do { ioc_.run_one(); } while (ec == boost::asio::error::would_block);
    if (ec)
        throw boost::system::system_error(ec);
}

void http_client::recv(http_res& res)
{
    boost::system::error_code ec = boost::asio::error::would_block;
    deadline_.expires_from_now(timeout_);
    boost::beast::http::async_read(socket_, buffer_, res,
        [&ec](boost::system::error_code e, std::size_t) { ec = e; });
    do { ioc_.run_one(); } while (ec == boost::asio::error::would_block);
    if (ec)
        throw boost::system::system_error(ec);
}

void http_client::start_deadline()
{
    deadline_.expires_at(std::chrono::system_clock::time_point::max());
    check_deadline();
}

void http_client::check_deadline()
{
    if (deadline_.expires_at() <= std::chrono::system_clock::now()) {
        try {
            socket_.cancel();
        } catch (const std::exception&) {
        }
        deadline_.expires_at(std::chrono::system_clock::time_point::max());
    }
    deadline_.async_wait(std::bind(&http_client::check_deadline, this));
}

void http_client::operator()(http_req& req, http_res& res)
{
    send(req);
    recv(res);
}

http_server::http_server(boost::asio::io_context& ioc, boost::asio::ip::tcp::endpoint ep,
    http_handler handler,
    std::chrono::seconds timeout)
  : ioc_(ioc), acceptor_(ioc, ep), socket_(ioc), deadline_(ioc), timeout_(timeout), handler_(handler)
{
    start_deadline();
    accept();
}

void http_server::cancel()
{
    acceptor_.cancel();
}

void http_server::accept()
{
    socket_ = boost::asio::ip::tcp::socket(ioc_);
    acceptor_.async_accept(socket_,
        [this](boost::system::error_code e) { if (!e) operator()(); accept(); });
}

void http_server::send(http_res& res)
{
    boost::system::error_code ec = boost::asio::error::would_block;
    deadline_.expires_from_now(timeout_);
    boost::beast::http::async_write(socket_, res,
        [&ec](boost::system::error_code e, std::size_t) { ec = e; });
    do { ioc_.run_one(); } while (ec == boost::asio::error::would_block);
    if (ec)
        throw boost::system::system_error(ec);
}

void http_server::recv(http_req& req)
{
    boost::system::error_code ec = boost::asio::error::would_block;
    deadline_.expires_from_now(timeout_);
    boost::beast::http::async_read(socket_, buffer_, req,
        [&ec](boost::system::error_code e, std::size_t) { ec = e; });
    do { ioc_.run_one(); } while (ec == boost::asio::error::would_block);
    if (ec)
        throw boost::system::system_error(ec);
}

void http_server::start_deadline()
{
    deadline_.expires_at(std::chrono::system_clock::time_point::max());
    check_deadline();
}

void http_server::check_deadline()
{
    if (deadline_.expires_at() <= std::chrono::system_clock::now()) {
        try {
            socket_.cancel();
        } catch (const std::exception&) {
        }
        deadline_.expires_at(std::chrono::system_clock::time_point::max());
    }
    deadline_.async_wait(std::bind(&http_server::check_deadline, this));
}

void http_server::operator()()
{
    http_req req;
    http_res res;
    recv(req);
    handler_(req, res);
    send(res);
}
