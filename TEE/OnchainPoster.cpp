#include "OnChainPoster.hpp"

void OnChainPoster::postFinancialDetails(
    const std::string &offerId,
    double price,
    int preferredBuyers,
    int expiryDays,
    int cooldownMonths,
    const std::string &fdePublicKey)
{
    std::cout << "=== OnChain Poster ===\n";
    std::cout << "Posting Offer [" << offerId << "] on chain with:\n"
              << "  Reserve Price: " << price << "\n"
              << "  Number of Buyers: " << preferredBuyers << "\n"
              << "  Expiry (days): " << expiryDays << "\n"
              << "  Cooldown (months): " << cooldownMonths << "\n"
              << "  FDE Public Key: " << fdePublicKey << "\n"
              << "== End OnChain Post ==\n\n";
}
