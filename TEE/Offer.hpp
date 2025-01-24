#ifndef OFFER_HPP
#define OFFER_HPP

#include <string>
#include <vector>

struct Offer
{
    std::string title;
    std::vector<std::string> verifiedKeywords;
    std::string unverifiedText;
    double reservePrice;
    int preferredNumberOfBuyers;
    int expiryDays;
    int cooldownMonths;
    std::string publicVerificationKeyFDE;
    std::string encryptedPlaintext;
    std::string nullifier;
};

#endif // OFFER_HPP
