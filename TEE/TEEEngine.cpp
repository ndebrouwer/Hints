#include "TEEEngine.hpp"
#include <iostream>
#include <optional>

TEEEngine::TEEEngine(const std::string &vkFilePath)
    : storage_(), validator_(vkFilePath), poster_()
{
}

bool TEEEngine::processOffer(
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
    const std::string &publicInputsPath)
{
    std::cout << "TEEEngine: Received new Offer [" << offerId << "]\n";

    bool isProofValid = validator_.validateOfferProof(proofPath, publicInputsPath);
    if (!isProofValid)
    {
        std::cerr << "TEEEngine: Proof invalid for Offer [" << offerId << "]. Rejecting.\n";
        return false;
    }

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

    poster_.postFinancialDetails(
        offerId,
        reservePrice,
        preferredBuyers,
        expiryDays,
        cooldownMonths,
        publicVerificationKeyFDE);

    storage_.storeOffer(offerId, off);

    std::cout << "TEEEngine: Successfully processed Offer [" << offerId << "].\n";
    return true;
}

Offer TEEEngine::getOffer(const std::string &offerId)
{
    auto maybeOffer = storage_.retrieveOffer(offerId);
    if (maybeOffer.has_value())
    {
        return maybeOffer.value();
    }
    return Offer(); // Or throw an exception if not found
}
