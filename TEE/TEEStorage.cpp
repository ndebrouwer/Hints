#include "TEEStorage.hpp"

void TEEStorage::storeOffer(const std::string &offerId, const Offer &offer)
{
    std::lock_guard<std::mutex> lock(mtx_);
    offers_[offerId] = offer;
}

std::optional<Offer> TEEStorage::retrieveOffer(const std::string &offerId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = offers_.find(offerId);
    if (it != offers_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> TEEStorage::listOfferIds()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> ids;
    for (const auto &kv : offers_)
    {
        ids.push_back(kv.first);
    }
    return ids;
}
