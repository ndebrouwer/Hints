pragma circom 2.2.1;
include "../node_modules/circomlib/circuits/bitify.circom";
include "../node_modules/circomlib/circuits/comparators.circom";
include "../node_modules/circomlib/circuits/mux1.circom";
include "../node_modules/circomlib/circuits/poseidon.circom";
include "../node_modules/@zk-email/zk-regex-circom/circuits/common/body_hash_regex.circom";
include "../node_modules/@zk-email/circuits/helpers/remove-soft-line-breaks.circom";
include "../node_modules/@zk-email/circuits/lib/base64.circom";
include "../node_modules/@zk-email/circuits/lib/rsa.circom";
include "../node_modules/@zk-email/circuits/lib/sha.circom";
include "../node_modules/@zk-email/circuits/utils/array.circom";
include "../node_modules/@zk-email/circuits/utils/regex.circom";
include "../node_modules/@zk-email/circuits/utils/hash.circom";
include "../node_modules/@zk-email/circuits/utils/bytes.circom";
include "../node_modules/@zk-email/circuits/email-verifier.circom";
include "../node_modules/@zk-email/circuits/helpers/email-nullifier.circom";
include "./PolyRollingHash.circom";
include "./RollingHashArray.circom";
include "./RabinKarp.circom";

//-------------------------------------
// Refactored EmailVerifierWithKeywords
//-------------------------------------
template EmailVerifierWithKeywords(
    maxHeadersLength,
    maxBodyLength,
    n,
    k,
    ignoreBodyHashCheck,
    enableHeaderMasking,
    enableBodyMasking,
    removeSoftLineBreaks,
    maxKeywords,
    maxKeywordLen,
    bitPerChunk,
    chunkSize
) {
    //EmailVerifier template inputs
    signal input emailHeader[maxHeadersLength];
    signal input emailHeaderLength;
    signal input pubkey[k];
    signal input signature[k];
    signal input emailBody[maxBodyLength];
    signal input emailBodyLength;
    signal input bodyHashIndex;
    signal input precomputedSHA[32];
    signal input decodedEmailBodyIn[maxBodyLength];
    signal input bodyMask[maxBodyLength];
    signal input headerMask[maxHeadersLength];

    // EmailVerifier outputs
    signal output pubkeyHash;
    signal output maskedHeader[maxHeadersLength];
    signal output maskedBody[maxBodyLength];
    signal output allKeywordsPresent;
    signal output nullifier;

    // Instantiate underlying EmailVerifier
    component emailVerifier = EmailVerifier(
        maxHeadersLength,
        maxBodyLength,
        n,
        k,
        ignoreBodyHashCheck,
        enableHeaderMasking,
        enableBodyMasking,
        removeSoftLineBreaks
    );

    // Connect signals
    emailVerifier.emailHeader <== emailHeader;
    emailVerifier.emailHeaderLength <== emailHeaderLength;
    emailVerifier.pubkey <== pubkey;
    emailVerifier.signature <== signature;

    if (ignoreBodyHashCheck != 1) {
        emailVerifier.bodyHashIndex <== bodyHashIndex;
        emailVerifier.precomputedSHA <== precomputedSHA;
        emailVerifier.emailBody <== emailBody;
        emailVerifier.emailBodyLength <== emailBodyLength;

        if (removeSoftLineBreaks == 1) {
            emailVerifier.decodedEmailBodyIn <== decodedEmailBodyIn;
        }
        if (enableBodyMasking == 1) {
            emailVerifier.bodyMask <== bodyMask;
        }
    }

    pubkeyHash <== emailVerifier.pubkeyHash;
    if (enableHeaderMasking == 1) {
        emailVerifier.headerMask <== headerMask;
        maskedHeader <== emailVerifier.maskedHeader;
    }
    if (enableBodyMasking == 1 && ignoreBodyHashCheck != 1) {
        maskedBody <== emailVerifier.maskedBody;
    }

    //RABIN KARP
    // 4) Instantiate RabinKarpKeywords to check for all keywords in the verified body
    signal input numKeywords;
    signal input keywords[maxKeywords][maxKeywordLen];
    var BASE = 256;

    component rabinKarp = RabinKarpKeywords(
        maxBodyLength,
        maxKeywordLen,
        maxKeywords,
        32,         // bitwidth for LessThan
        BASE
    );

    for (var i = 0; i < maxBodyLength; i++) {
        rabinKarp.emailBody[i] <== emailBody[i];
    }
    for (var i = 0; i < maxKeywords; i++) {
        for (var c = 0; c < maxKeywordLen; c++) {
            rabinKarp.keywords[i][c] <== keywords[i][c];
        }
    }
    rabinKarp.numKeywords <== numKeywords;
    allKeywordsPresent <== rabinKarp.allKeywordsPresent;

    //NULLIFIER   
    component emailNullifier = EmailNullifier(bitPerChunk, chunkSize);
    for (var i = 0; i < chunkSize; i++) {
        emailNullifier.signature[i] <== signature[i];
    }
    nullifier <== emailNullifier.out;
    //log("EmailVerifierWithKeywords refactored: finished nullifier");

}
