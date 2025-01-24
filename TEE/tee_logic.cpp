/************************************************************
 * Example C++ code for a TEE service that "listens" for 
 * incoming offers (via WebSocket, etc.), verifies proofs 
 * of email + keywords, and then posts the offer data. 
 * 
 * NOTE: This is a simplified, illustrative example and is 
 * not production-grade. Many security concerns are omitted. 
 * 
 * Compilation instructions (example):
 *   g++ -std=c++17 tee_service.cpp -o tee_service \
 *       -lstdc++fs -lpthread 
 * 
 * Requires WebSocket or networking libraries of your choice. 
 * For illustration, we use standalone Boost.Beast/ASIO style 
 * pseudo-code (not all includes or error checks shown). 
 ************************************************************/

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <sstream>
#include <optional>

// JSON library (e.g. nlohmann/json)
#include <nlohmann/json.hpp>
using json = nlohmann::json;

//-----------------------------------------------
// ZK-SNARK Verification Components
//-----------------------------------------------
#include <libsnark/common/default_types/r1cs_ppzksnark_pp.hpp>
#include <libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp>
#include <fstream>

// For brevity, we keep these in the same file. 
// In a real project, factor them out into dedicated modules.

using namespace libsnark;
using curve_pp = default_r1cs_ppzksnark_pp;

// Helper function: load the verification key
r1cs_ppzksnark_verification_key<curve_pp> loadVerificationKey(const std::string &file_path)
{
    std::ifstream vk_file(file_path, std::ios::binary);
    if (!vk_file)
    {
        throw std::runtime_error("Unable to open verification key file: " + file_path);
    }

    r1cs_ppzksnark_verification_key<curve_pp> vk;
    vk_file >> vk;
    vk_file.close();
    return vk;
}

// Helper function: load proof
r1cs_ppzksnark_proof<curve_pp> loadProof(const std::string &file_path)
{
    std::ifstream proof_file(file_path, std::ios::binary);
    if (!proof_file)
    {
        throw std::runtime_error("Unable to open proof file: " + file_path);
    }

    r1cs_ppzksnark_proof<curve_pp> proof;
    proof_file >> proof;
    proof_file.close();
    return proof;
}

// Helper function: load public inputs from JSON
std::vector<r1cs_ppzksnark_primary_input<curve_pp>> loadPublicInputs(const std::string &file_path)
{
    std::ifstream input_file(file_path);
    if (!input_file)
    {
        throw std::runtime_error("Unable to open public input file: " + file_path);
    }

    json input_json;
    input_file >> input_json;
    input_file.close();

    std::vector<r1cs_ppzksnark_primary_input<curve_pp>> public_inputs;
    for (const auto &signal : input_json)
    {
        // The JSON structure must be consistent with the libsnark format. 
        // For simplicity, we assume each signal is an integer in a string. 
        // Or you might do something more advanced.  
        // This block is purely illustrative. 
        r1cs_ppzksnark_primary_input<curve_pp> pi;
        pi.emplace_back(bigint<libff::Fr<curve_pp>::num_limbs>(signal.get<std::string>().c_str()));
        public_inputs.push_back(pi);
    }
    return public_inputs;
}

// Actual proof verification
bool verifyProof(
    const r1cs_ppzksnark_verification_key<curve_pp> &vk,
    const r1cs_ppzksnark_proof<curve_pp> &proof,
    const std::vector<r1cs_ppzksnark_primary_input<curve_pp>> &primary_input)
{
    // For a typical circuit, there's exactly one primary input 
    // or one array of them. We'll do a simple approach:
    if (primary_input.empty())
    {
        return false;
    }

    // If there's only one set of public inputs in your scenario:
    return r1cs_ppzksnark_verifier_strong_IC<curve_pp>(
        vk, primary_input[0], proof);
}

//-----------------------------------------------
// Data Structures for an "Offer"
//-----------------------------------------------
struct Offer
{
    // The user-supplied Title (public string)
    std::string title;

    // The verified keywords (strings). 
    // The TEE verifies the presence of these in the email proof
    std::vector<std::string> verifiedKeywords;

    // The unverified (public) text disclosure
    std::string unverifiedText;

    // The user's reserve price, number of buyers, expiry, cooldown 
    double reservePrice;
    int preferredNumberOfBuyers;
    int expiryDays;     // how many days until the offer expires
    int cooldownMonths; // how many months cooldown
    std::string publicVerificationKeyFDE;

    // The raw encryption data, presumably the commitment "C"
    // or the encrypted plaintext. We just store it. 
    std::string encryptedPlaintext;

    // The recorded nullifier, used internally (not posted)
    // We'll store it but keep it separate from the posted data
    std::string nullifier;
};

//-----------------------------------------------
// TEE Storage class
//-----------------------------------------------
class TEEStorage
{
private:
    std::mutex mtx_;
    // Simple in-memory storage of offers. Real apps might use 
    // a secure enclave or secure DB. 
    std::map<std::string, Offer> offers_; // key by some offer ID

public:
    TEEStorage() = default;

    void storeOffer(const std::string &offerId, const Offer &offer)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        offers_[offerId] = offer;
    }

    std::optional<Offer> retrieveOffer(const std::string &offerId)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = offers_.find(offerId);
        if (it != offers_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    // For demonstration: list all offers stored
    std::vector<std::string> listOfferIds()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> ids;
        for (const auto &kv : offers_)
        {
            ids.push_back(kv.first);
        }
        return ids;
    }
};

//-----------------------------------------------
// OfferValidator class
// - Verifies the user-supplied proof of email 
//   and presence of keywords using the 
//   r1cs_ppzksnark verification approach
// - This code references the snippet above
//-----------------------------------------------
class OfferValidator
{
private:
    // Load or store the verification key used for the 
    // "Email + Keyword" proof circuit. 
    r1cs_ppzksnark_verification_key<curve_pp> vk_;

public:
    OfferValidator(const std::string &vkFilePath)
    {
        curve_pp::init_public_params();
        vk_ = loadVerificationKey(vkFilePath);
    }

    bool validateOfferProof(
        const std::string &proofPath,
        const std::string &publicInputsPath)
    {
        try
        {
            // Load the proof
            auto proof = loadProof(proofPath);

            // Load the public inputs (should correspond to 
            // domain from or to, plus hashed keywords, etc.)
            auto pubInputs = loadPublicInputs(publicInputsPath);

            // Verify
            bool isValid = verifyProof(vk_, proof, pubInputs);
            return isValid;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error during proof validation: " << e.what() << std::endl;
            return false;
        }
    }
};

//-----------------------------------------------
// OnChainPoster 
// - Stub for posting partial data on-chain
//   We only post minimal financial info (price, nOfBuyers, 
//   expiry, public verification key, cooldown).
//-----------------------------------------------
class OnChainPoster
{
public:
    // Dummy function that simulates a blockchain post
    // In real code, you might use a blockchain SDK.
    void postFinancialDetails(
        const std::string &offerId,
        double price,
        int preferredBuyers,
        int expiryDays,
        int cooldownMonths,
        const std::string &fdePublicKey)
    {
        // Example: just print to console 
        std::cout << "=== OnChain Poster ===\n";
        std::cout << "Posting Offer [" << offerId << "] on chain with:\n"
                  << "  Reserve Price: " << price << "\n"
                  << "  Number of Buyers: " << preferredBuyers << "\n"
                  << "  Expiry (days): " << expiryDays << "\n"
                  << "  Cooldown (months): " << cooldownMonths << "\n"
                  << "  FDE Public Key: " << fdePublicKey << "\n"
                  << "== End OnChain Post ==\n\n";
    }
};

//-----------------------------------------------
// TEEEngine 
// - Orchestrates receiving the Offer, 
//   validating, storing, and posting on-chain
//-----------------------------------------------
class TEEEngine
{
private:
    TEEStorage storage_;
    OfferValidator validator_;
    OnChainPoster poster_;

public:
    // Constructor takes path to verification key for email+keyword proof
    TEEEngine(const std::string &vkFilePath)
        : storage_(), validator_(vkFilePath), poster_()
    {
    }

    // The core function that processes an incoming Offer 
    // from the outside world 
    bool processOffer(
        const std::string &offerId,
        const std::string &title,
        const std::vector<std::string> &verifiedKeywords,
        const std::string &unverifiedText,
        double reservePrice,
        int preferredBuyers,
        int expiryDays,
        int cooldownMonths,
        const std::string &publicVerificationKeyFDE,
        const std::string &encryptedPlaintext,
        const std::string &nullifier,
        // Proof file paths (for demonstration)
        const std::string &proofPath,
        const std::string &publicInputsPath)
    {
        std::cout << "TEEEngine: Received new Offer [" << offerId << "]\n";

        // 1) Strip and record the nullifier 
        //    (Here we just record it in the Offer struct)
        // 2) Verify the email proof + keywords 
        bool isProofValid = validator_.validateOfferProof(proofPath, publicInputsPath);
        if (!isProofValid)
        {
            std::cerr << "TEEEngine: Proof invalid for Offer [" << offerId << "]. Rejecting.\n";
            return false;
        }

        // 3) If valid, store the encryptedPlaintext, the public verification key 
        //    (for the "Fair Data Exchange" system)
        Offer off;
        off.title = title;
        off.verifiedKeywords = verifiedKeywords;
        off.unverifiedText = unverifiedText;
        off.reservePrice = reservePrice;
        off.preferredNumberOfBuyers = preferredBuyers;
        off.expiryDays = expiryDays;
        off.cooldownMonths = cooldownMonths;
        off.publicVerificationKeyFDE = publicVerificationKeyFDE;
        off.encryptedPlaintext = encryptedPlaintext;
        off.nullifier = nullifier;

        // 4) Post partial details (financial) on-chain 
        poster_.postFinancialDetails(
            offerId,
            reservePrice,
            preferredBuyers,
            expiryDays,
            cooldownMonths,
            publicVerificationKeyFDE);

        // 5) Store the entire Offer in TEE storage
        storage_.storeOffer(offerId, off);

        std::cout << "TEEEngine: Successfully processed Offer [" << offerId << "].\n";
        return true;
    }

    // Query the TEE storage for a known Offer's data (title, tags, etc.)
    std::optional<Offer> getOffer(const std::string &offerId)
    {
        return storage_.retrieveOffer(offerId);
    }
};

//-----------------------------------------------
// Minimal WebSocket server or HTTP server example
// - This is a simplified illustration using Boost.Beast 
//   style pseudo-code (not tested). 
//-----------------------------------------------

#ifdef USE_BOOST_BEAST // compile-time guard

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <functional>

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// A session that manages communication for one connection
class Session : public std::enable_shared_from_this<Session>
{
private:
    websocket::stream<tcp::socket> ws_;
    TEEEngine &engine_; // reference to our TEEEngine

public:
    explicit Session(tcp::socket socket, TEEEngine &engine)
        : ws_(std::move(socket)), engine_(engine)
    {
    }

    // Start the asynchronous operation
    void run()
    {
        // Accept the WebSocket handshake
        auto self = shared_from_this();
        ws_.async_accept(
            [self](boost::beast::error_code ec) {
                if (!ec)
                    self->doRead();
            });
    }

    void doRead()
    {
        auto self = shared_from_this();
        boost::beast::flat_buffer buffer;
        ws_.async_read(
            buffer,
            [self, &buffer](boost::beast::error_code ec, std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);

                if (ec)
                {
                    std::cerr << "WebSocket read error: " << ec.message() << std::endl;
                    return;
                }

                // Parse the incoming message as JSON
                auto data = boost::beast::buffers_to_string(buffer.data());
                json j;
                try
                {
                    j = json::parse(data);
                }
                catch (...)
                {
                    // If parse fails, we handle it
                    std::cerr << "Invalid JSON received.\n";
                    return;
                }

                // We'll assume the JSON structure:
                // {
                //   "offerId": "...",
                //   "title": "...",
                //   "verifiedKeywords": ["kw1", "kw2", ...],
                //   "unverifiedText": "...",
                //   "reservePrice": 5.0,
                //   "preferredBuyers": 5,
                //   "expiryDays": 10,
                //   "cooldownMonths": 3,
                //   "publicVerificationKeyFDE": "...",
                //   "encryptedPlaintext": "...",
                //   "nullifier": "...",
                //   "proofPath": "/secure_path/proof.bin",
                //   "publicInputsPath": "/secure_path/public_inputs.json"
                // }

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

                // Convert verifiedKeywords from JSON array to vector<std::string>
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

                // For demonstration, write a response 
                // "OK" if success, "ERROR" otherwise
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

                // Echo the result to the client
                self->ws_.text(true);
                self->ws_.async_write(
                    boost::asio::buffer(response.dump()),
                    [self](boost::beast::error_code ec, std::size_t)
                    {
                        if (ec)
                            std::cerr << "WebSocket write error: " << ec.message() << std::endl;
                    });
                // read next message
                self->doRead();
            });
    }
};

class Listener : public std::enable_shared_from_this<Listener>
{
private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    TEEEngine &engine_;

public:
    Listener(boost::asio::io_context &ioc, tcp::endpoint endpoint, TEEEngine &engine)
        : acceptor_(ioc), socket_(ioc), engine_(engine)
    {
        boost::system::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    }

    void run()
    {
        doAccept();
    }

    void doAccept()
    {
        auto self = shared_from_this();
        acceptor_.async_accept(
            socket_,
            [self](boost::system::error_code ec) {
                if (!ec)
                {
                    // Create the session and run it
                    std::make_shared<Session>(std::move(self->socket_), self->engine_)->run();
                }
                self->doAccept();
            });
    }
};

#endif // USE_BOOST_BEAST

//-----------------------------------------------
// main() - Example usage
//-----------------------------------------------
int main(int argc, char *argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: " << argv[0] << " <vkFilePath>\n";
            std::cerr << "Optionally define USE_BOOST_BEAST at compile time for the server.\n";
            return 1;
        }

        // Path to the verification key (for the email+keyword circuit)
        std::string vkPath = argv[1];

        // Create the TEEEngine
        TEEEngine engine(vkPath);

#ifdef USE_BOOST_BEAST
        // If we compile with a WebSocket server, run it
        boost::asio::io_context ioc;
        tcp::endpoint endpoint(tcp::v4(), 8080); // listen on port 8080
        std::make_shared<Listener>(ioc, endpoint, engine)->run();

        std::cout << "TEE listening on ws://0.0.0.0:8080\n";
        ioc.run(); // blocking call
#else
        // If we didn't compile with the server, just do a local test

        // Suppose we have an offer to test with local paths 
        // (no real server needed).
        std::string dummyOfferId = "offer123";
        std::string dummyTitle = "Corporate misconduct by a large mining corporation";
        std::vector<std::string> dummyKeywords = {
            "EPA", "fine", "quarterly earnings", "production", 
            "liable", "compensation", "DOJ", "compliance"};
        std::string unverifiedText =
            "A large American mining giant whose name can be deduced "
            "from the plaintext is going to be fined by the DOJ soon.";
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
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
