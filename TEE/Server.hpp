#ifndef WEBSOCKETSERVER_HPP
#define WEBSOCKETSERVER_HPP

#ifdef USE_BOOST_BEAST

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <string>
#include <vector>
#include "TEEEngine.hpp"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

class Session : public std::enable_shared_from_this<Session>
{
private:
    websocket::stream<tcp::socket> ws_;
    TEEEngine &engine_;

    void doRead();

public:
    explicit Session(tcp::socket socket, TEEEngine &engine);
    void run();
};

class Listener : public std::enable_shared_from_this<Listener>
{
private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    TEEEngine &engine_;

    void doAccept();

public:
    Listener(boost::asio::io_context &ioc, tcp::endpoint endpoint, TEEEngine &engine);
    void run();
};

#endif // USE_BOOST_BEAST

#endif // WEBSOCKETSERVER_HPP
