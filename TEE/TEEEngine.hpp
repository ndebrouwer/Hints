#ifndef TEEENGINE_HPP
#define TEEENGINE_HPP

#include <string>
#include <vector>
#include "Offer.hpp"
#include "TEEStorage.hpp"
#include "OfferValidator.hpp"
#include "OnChainPoster.hpp"

class TEEEngine
{
private:
    TEEStorage storage_;
    OfferValidator validator_;
    OnChainPoster poster_;

public:
    explicit TEEEngine(const std::string &vkFilePath);

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
        const std::string &proofPath,
        const std::string &publicInputsPath);

    // Retrieve a stored Offer
    Offer getOffer(const std::string &offerId);
};

#endif // TEEENGINE_HPP
