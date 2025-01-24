#ifndef ONCHAINPOSTER_HPP
#define ONCHAINPOSTER_HPP

#include <string>
#include <iostream>

class OnChainPoster
{
public:
    void postFinancialDetails(
        const std::string &offerId,
        double price,
        int preferredBuyers,
        int expiryDays,
        int cooldownMonths,
        const std::string &fdePublicKey);
};

#endif // ONCHAINPOSTER_HPP
