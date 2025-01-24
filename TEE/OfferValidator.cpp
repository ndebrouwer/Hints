#include "OfferValidator.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>

using namespace libsnark;
using curve_pp = default_r1cs_ppzksnark_pp;
using json = nlohmann::json;

/*************************
 * Helper Functions
 ************************/
static r1cs_ppzksnark_verification_key<curve_pp> loadVerificationKey(const std::string &file_path)
{
    std::ifstream vk_file(file_path, std::ios::binary);
    if (!vk_file)
        throw std::runtime_error("Unable to open verification key file: " + file_path);

    r1cs_ppzksnark_verification_key<curve_pp> vk;
    vk_file >> vk;
    vk_file.close();
    return vk;
}

static r1cs_ppzksnark_proof<curve_pp> loadProof(const std::string &file_path)
{
    std::ifstream proof_file(file_path, std::ios::binary);
    if (!proof_file)
        throw std::runtime_error("Unable to open proof file: " + file_path);

    r1cs_ppzksnark_proof<curve_pp> proof;
    proof_file >> proof;
    proof_file.close();
    return proof;
}

static std::vector<r1cs_ppzksnark_primary_input<curve_pp>>
loadPublicInputs(const std::string &file_path)
{
    std::ifstream input_file(file_path);
    if (!input_file)
        throw std::runtime_error("Unable to open public input file: " + file_path);

    json input_json;
    input_file >> input_json;
    input_file.close();

    std::vector<r1cs_ppzksnark_primary_input<curve_pp>> public_inputs;
    for (const auto &signal : input_json)
    {
        r1cs_ppzksnark_primary_input<curve_pp> pi;
        pi.emplace_back(
            bigint<libff::Fr<curve_pp>::num_limbs>(signal.get<std::string>().c_str()));
        public_inputs.push_back(pi);
    }
    return public_inputs;
}

static bool verifyProof(
    const r1cs_ppzksnark_verification_key<curve_pp> &vk,
    const r1cs_ppzksnark_proof<curve_pp> &proof,
    const std::vector<r1cs_ppzksnark_primary_input<curve_pp>> &primary_input)
{
    if (primary_input.empty())
        return false;
    return r1cs_ppzksnark_verifier_strong_IC<curve_pp>(
        vk, primary_input[0], proof);
}

/*************************
 * OfferValidator Methods
 ************************/
OfferValidator::OfferValidator(const std::string &vkFilePath)
{
    curve_pp::init_public_params();
    vk_ = loadVerificationKey(vkFilePath);
}

bool OfferValidator::validateOfferProof(const std::string &proofPath,
                                        const std::string &publicInputsPath)
{
    try
    {
        auto proof = loadProof(proofPath);
        auto pubInputs = loadPublicInputs(publicInputsPath);
        return verifyProof(vk_, proof, pubInputs);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during proof validation: " << e.what() << std::endl;
        return false;
    }
}
