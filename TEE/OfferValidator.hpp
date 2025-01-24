#ifndef OFFERVALIDATOR_HPP
#define OFFERVALIDATOR_HPP

#include <string>
#include <libsnark/common/default_types/r1cs_ppzksnark_pp.hpp>
#include <libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp>

class OfferValidator
{
private:
    libsnark::r1cs_ppzksnark_verification_key<
        libsnark::default_r1cs_ppzksnark_pp> vk_;

public:
    explicit OfferValidator(const std::string &vkFilePath);

    bool validateOfferProof(
        const std::string &proofPath,
        const std::string &publicInputsPath);
};

#endif // OFFERVALIDATOR_HPP
