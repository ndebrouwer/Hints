#ifdef USE_BOOST_BEAST

#include "WebSocketServer.hpp"
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

Session::Session(tcp::socket socket, TEEEngine &engine)
    : ws_(std::move(socket)), engine_(engine)
{
}

void Session::run()
{
    auto self = shared_from_this();
    ws_.async_accept([self](boost::beast::error_code ec) {
        if (!ec) self->doRead();
    });
}

void Session::doRead()
{
    auto self = shared_from_this();
    boost::beast::flat_buffer buffer;
    ws_.async_read(buffer, [self, &buffer](boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec)
        {
            std::cerr << "WebSocket read error: " << ec.message() << std::endl;
            return;
        }

        auto data = boost::beast::buffers_to_string(buffer.data());
        json j;
        try
        {
            j = json::parse(data);
        }
        catch (...)
        {
            std::cerr << "Invalid JSON received.\n";
            return;
        }

        // Minimal handling of the fields
        std::string offerId = j.value("offerId", "unknown_offer");
        std::string title = j.value("title", "");
        auto verifiedKeywordsJson = j.value("verifiedKeywords", std::vector<std::string>{});
        std::string unverifiedText = j.value("unverifiedText", "");
        double reservePrice = j.value("reservePrice", 0.0);
        int buyers = j.value("preferredBuyers", 0);
        int expiryDays = j.value("expiryDays", 0);
        int cooldownMonths = j.value("cooldownMonths", 0);
        std::string pvkFDE = j.value("publicVerificationKeyFDE", "");
        std::string encPlaintext = j.value("encryptedPlaintext", "");
        std::string nullifier = j.value("nullifier", "");
        std::string proofPath = j.value("proofPath", "");
        std::string publicInputsPath = j.value("publicInputsPath", "");

        std::vector<std::string> verifiedKeywords;
        for (auto &kw : verifiedKeywordsJson)
        {
            verifiedKeywords.push_back(kw);
        }

        bool success = self->engine_.processOffer(
            offerId,
            title,
            verifiedKeywords,
            unverifiedText,
            reservePrice,
            buyers,
            expiryDays,
            cooldownMonths,
            pvkFDE,
            encPlaintext,
            nullifier,
            proofPath,
            publicInputsPath);

        json response;
        if (success)
        {
            response["status"] = "OK";
            response["offerId"] = offerId;
        }
        else
        {
            response["status"] = "ERROR";
            response["message"] = "Proof invalid or other error.";
        }

        self->ws_.text(true);
        self->ws_.async_write(
            boost::asio::buffer(response.dump()),
            [self](boost::beast::error_code ec, std::size_t)
            {
                if (ec)
                    std::cerr << "WebSocket write error: " << ec.message() << std::endl;
            });

        self->doRead();
    });
}

Listener::Listener(boost::asio::io_context &ioc, tcp::endpoint endpoint, TEEEngine &engine)
    : acceptor_(ioc), socket_(ioc), engine_(engine)
{
    boost::system::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
}

void Listener::run()
{
    doAccept();
}

void Listener::doAccept()
{
    auto self = shared_from_this();
    acceptor_.async_accept(socket_, [self](boost::system::error_code ec) {
        if (!ec)
        {
            std::make_shared<Session>(std::move(self->socket_), self->engine_)->run();
        }
        self->doAccept();
    });
}

#endif // USE_BOOST_BEAST
