#include <iostream>
#include "TEEEngine.hpp"

#ifdef USE_BOOST_BEAST
#include "WebSocketServer.hpp"
#include <boost/asio.hpp>
#endif

int main(int argc, char *argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: " << argv[0] << " <vkFilePath>\n";
            return 1;
        }

        std::string vkPath = argv[1];

        TEEEngine engine(vkPath);

#ifdef USE_BOOST_BEAST
        boost::asio::io_context ioc;
        using tcp = boost::asio::ip::tcp;
        tcp::endpoint endpoint(tcp::v4(), 8080);
        auto listener = std::make_shared<Listener>(ioc, endpoint, engine);
        listener->run();

        std::cout << "TEE listening on ws://0.0.0.0:8080\n";
        ioc.run();
#else
        // Local test if not compiled with the server
        std::string dummyOfferId = "offer123";
        std::string dummyTitle = "Corporate misconduct by a large mining corporation";
        std::vector<std::string> dummyKeywords = {
            "EPA", "fine", "quarterly earnings", "production", 
            "liable", "compensation", "DOJ", "compliance"};
        std::string unverifiedText =
            "A large American mining giant...";
        double reservePrice = 5.0;
        int buyers = 5;
        int expiryDays = 10;
        int cooldown = 3;
        std::string fdeKey = "random_gibberish_key";
        std::string encPlaintext = "EncryptedDataHere";
        std::string nullifier = "some_nullifier";
        std::string proofPath = "/secure_path/proof.bin";
        std::string publicInputsPath = "/secure_path/public_inputs.json";

        bool success = engine.processOffer(
            dummyOfferId,
            dummyTitle,
            dummyKeywords,
            unverifiedText,
            reservePrice,
            buyers,
            expiryDays,
            cooldown,
            fdeKey,
            encPlaintext,
            nullifier,
            proofPath,
            publicInputsPath
        );

        std::cout << "Local test Offer processing result: "
                  << (success ? "SUCCESS" : "FAILURE") << "\n";
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
