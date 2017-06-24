//
// Copyright (c) 2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "example/common/file_body.hpp"
#include "example/common/mime_types.hpp"
#include "example/common/rfc7231.hpp"
#include "example/common/session_alloc.hpp"
#include "example/common/write_msg.hpp"

#include "../common/mime_types.hpp"

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <string>

namespace ip = boost::asio::ip; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio.hpp>
namespace http = beast::http; // from <beast/http.hpp>

class http_worker
{
public:
    http_worker(http_worker const&) = delete;
    http_worker& operator=(http_worker const&) = delete;

    http_worker(tcp::acceptor& acceptor, const std::string& doc_root) :
        acceptor_(acceptor),
        doc_root_(doc_root),
        socket_(acceptor.get_io_service()),
        request_deadline_(acceptor.get_io_service(),
            std::chrono::steady_clock::time_point::max())
    {
    }

    void start()
    {
        accept();
        check_deadline();
    }

private:
    using alloc_type = session_alloc<char>;
    using request_body_t = http::basic_dynamic_body<beast::static_buffer_n<1024 * 1024>>;
    using parser_type = http::request_parser<request_body_t, alloc_type>;
    using request_type = typename parser_type::value_type;

    // The acceptor used to listen for incoming connections.
    tcp::acceptor& acceptor_;

    // The path to the root of the document directory.
    std::string doc_root_;

    // The socket for the currently connected client.
    tcp::socket socket_;

    // The buffer for performing reads
    beast::static_buffer_n<8192> buffer_;

    alloc_type alloc_;

    // The parser for reading the requests
    boost::optional<parser_type> parser_;

    // The timer putting a time limit on requests.
    boost::asio::basic_waitable_timer<std::chrono::steady_clock> request_deadline_;

    void
    accept()
    {
        // Clean up any previous connection.
        beast::error_code ec;
        socket_.close(ec);
        buffer_.consume(buffer_.size());

        acceptor_.async_accept(
            socket_,
            [this](beast::error_code ec)
            {
                if (ec)
                {
                    accept();
                }
                else
                {
                    // Request must be fully processed within 60 seconds.
                    request_deadline_.expires_from_now(
                        std::chrono::seconds(60));

                    read_header();
                }
            });
    }

    void
    fail(beast::error_code const& ec)
    {
beast::unit_test::dstream dout{std::cout};
dout << "error: " << ec.message() << std::endl;
    }

    void
    read_header()
    {
        // On each read the parser needs to be destroyed and
        // recreated. We store it in a boost::optional to
        // achieve that.
        //
        // Arguments passed to the parser constructor are
        // forwarded to the message object. A single argument
        // is forwarded to the body constructor.
        //
        // We construct the dynamic body with a 1MB limit
        // to prevent vulnerability to buffer attacks.
        //
        parser_.emplace(
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(alloc_));

        http::async_read_header(
            socket_,
            buffer_,
            *parser_,
            alloc_.wrap([this](beast::error_code ec)
            {
                if(ec && ec != http::error::end_of_stream)
                    fail(ec);
                if (ec)
                    accept();
                else
                    read_body();
            }));
    }

    void read_body()
    {
        http::async_read(
            socket_,
            buffer_,
            *parser_,
            alloc_.wrap([this](beast::error_code ec)
            {
                if (ec)
                    return accept();
                process_request(parser_->get());
            }));
    }

    template<bool isRequest, class Body, class Fields>
    void
    send(http::message<isRequest, Body, Fields>&& msg)
    {
        if(rfc7231::is_keep_alive(msg))
            read_header();
        async_write_msg(socket_, std::move(msg), alloc_.wrap(
            [this](beast::error_code ec)
            {
                if(ec)
                {
                    if(ec == http::error::end_of_stream)
                        socket_.shutdown(tcp::socket::shutdown_send, ec);
                    return accept();
                }
            }));
    }

    void
    process_request(request_type const& req)
    {
        switch (req.method())
        {
        case http::verb::get:
        {
            boost::filesystem::path path = doc_root_;
            if(req.target() == "/")
                path /= "index.html";
            else
                path /= req.target().to_string();
            if(boost::filesystem::exists(path))
            {
                http::response<
                    file_body,
                    http::basic_fields<alloc_type>> res(path, alloc_);
                res.version = req.version;
                res.result(http::status::ok);
                res.set(http::field::server, BEAST_VERSION_STRING);
                res.set(http::field::content_type, mime_type(path));
                res.prepare_payload();
                send(std::move(res));
            }
            else
            {
                std::basic_string<char, std::char_traits<char>, alloc_type> s(alloc_);
                s.append("File '");
                s.append(req.target().data(), req.target().size());
                s.append("' was not found");
                send(make_error_response(req, http::status::bad_request, beast::string_view{s.data(), s.size()}));
            }
            break;
        }

        default:
        {
            std::basic_string<char, std::char_traits<char>, alloc_type> s(alloc_);
            s.append("Invalid request-method '");
            auto const ms = req.method_string();
            s.append(ms.data(), ms.size());
            s.append("'");
            send(make_error_response(req, http::status::bad_request, beast::string_view{s.data(), s.size()}));
            break;
        }
        }
<<<<<<< HEAD

        // Fill out the reply to be sent to the client.
        response_.set(http::field::content_type, mime_type(target.to_string()));
        response_.body.clear();
        for (char buf[2048]; is.read(buf, sizeof(buf)).gcount() > 0;)
            response_.body.append(buf, static_cast<std::size_t>(is.gcount()));
        response_.prepare_payload();
=======
>>>>>>> 3cbdc059... [WIP] Add session_alloc to http-server-fast
    }

    template<class Body, class Fields>
    http::response<
        http::basic_dynamic_body<beast::basic_multi_buffer<alloc_type>>,
        http::basic_fields<alloc_type>>
    make_error_response(http::request<Body, Fields> const& req,
        http::status code, beast::string_view text)
    {
        http::response<
            http::basic_dynamic_body<beast::basic_multi_buffer<alloc_type>>,
            http::basic_fields<alloc_type>> res{alloc_, alloc_};
        res.version = req.version;
        res.result(code);
        res.set(http::field::server, BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/plain");
        beast::ostream(res.body) << text;
        res.prepare_payload();
        return res;
    }

    void check_deadline()
    {
        // The deadline may have moved, so check it has really passed.
        if (request_deadline_.expires_at() <= std::chrono::steady_clock::now())
        {
            // Close socket to cancel any outstanding operation.
            beast::error_code ec;
            socket_.close();

            // Sleep indefinitely until we're given a new deadline.
            request_deadline_.expires_at(
                std::chrono::steady_clock::time_point::max());
        }

        request_deadline_.async_wait(
            [this](beast::error_code)
            {
                check_deadline();
            });
    }
};

int main(int argc, char* argv[])
{
    try
    {
        // Check command line arguments.
        if (argc != 5)
        {
            std::cerr << "Usage: http_server <address> <port> <doc_root> <num_workers>\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    receiver 0.0.0.0 80 . 100\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    receiver 0::0 80 . 100\n";
            return EXIT_FAILURE;
        }

        auto address = ip::address::from_string(argv[1]);
        unsigned short port = static_cast<unsigned short>(std::atoi(argv[2]));
        std::string doc_root = argv[3];
        int num_workers = std::atoi(argv[4]);

        boost::asio::io_service ios{1};
        tcp::acceptor acceptor{ios, {address, port}};

        std::list<http_worker> workers;
        for (int i = 0; i < num_workers; ++i)
        {
            workers.emplace_back(acceptor, doc_root);
            workers.back().start();
        }

        ios.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
