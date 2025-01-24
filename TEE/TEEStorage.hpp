#ifndef TEESTORAGE_HPP
#define TEESTORAGE_HPP

#include <map>
#include <string>
#include <mutex>
#include <optional>
#include <vector>
#include "Offer.hpp"

class TEEStorage
{
private:
    std::mutex mtx_;
    std::map<std::string, Offer> offers_;

public:
    TEEStorage() = default;

    void storeOffer(const std::string &offerId, const Offer &offer);

    std::optional<Offer> retrieveOffer(const std::string &offerId);

    std::vector<std::string> listOfferIds();
};

#endif // TEESTORAGE_HPP
