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

/// @title EmailVerifierWithKeywords
/// @notice Extends the original EmailVerifier to also do naive substring checks on the verified body.
/// @dev The circuit will fail if the DKIM signature or body-hash check fails.
///      If all checks pass, you additionally get a boolean signal "allKeywordsPresent"
///      indicating if every given keyword appears in the body at least once.
///
/// @param maxHeadersLength  Maximum length for the email header
/// @param maxBodyLength     Maximum length for the email body
/// @param n                 Number of bits per chunk the RSA key is split into. (e.g., 121)
/// @param k                 Number of chunks the RSA key is split into. (e.g., 17)
/// @param ignoreBodyHashCheck  1 = skip checking the bh=, 0 = require it
/// @param enableHeaderMasking   1 = enable header masking
/// @param enableBodyMasking     1 = enable body masking
/// @param removeSoftLineBreaks  1 = remove QP-style soft line breaks in the body
///
/// @param maxKeywords       The maximum number of keywords the circuit can handle at runtime
/// @param maxKeywordLen     The maximum length (in bytes) of each keyword
///
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
    maxKeywordLen
) {
    // -----------------------------------------
    // 1) Declare inputs for EmailVerifier
    // -----------------------------------------
    signal input emailHeader[maxHeadersLength];
    signal input emailHeaderLength;
    signal input pubkey[k];
    signal input signature[k];
    signal input bodyHashIndex;
    signal input precomputedSHA[32];       // or 20 if SHA-1
    signal input emailBody[maxBodyLength];
    signal input emailBodyLength;
    signal input decodedEmailBodyIn[maxBodyLength];
    signal input bodyMask[maxBodyLength];

    // -----------------------------------------
    // 2) Outputs from EmailVerifier
    // -----------------------------------------
    signal output pubkeyHash;
    signal output maskedHeader[maxHeadersLength];
    signal output maskedBody[maxBodyLength];

    // -----------------------------------------
    // 3) Instantiate the EmailVerifier
    // -----------------------------------------
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

    // Connect signals ...
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
        maskedHeader <== emailVerifier.maskedHeader;
    }
    if (enableBodyMasking == 1 && ignoreBodyHashCheck != 1) {
        maskedBody <== emailVerifier.maskedBody;
    }

    // -----------------------------------------
    // 4) Keyword-checking signals
    // -----------------------------------------
    signal input numKeywords;
    signal input keywords[maxKeywords][maxKeywordLen];
    signal output allKeywordsPresent;

    // A) For substring checks, we hold temporary "match" signals:
    //    matchArr[i][j] = 1 if keyword i matches emailBody at position j, else 0
    signal matchArr[maxKeywords][maxBodyLength - maxKeywordLen + 1];
    // Whether each keyword was found at least once
    signal keywordFound[maxKeywords];

    // B) We'll build an OR-chain to see if each keyword i appears anywhere in the body.
    //    orChain[i][j+1] = orChain[i][j] OR matchArr[i][j].
    //
    //    For convenience, we declare a 2D array:
    signal orChain[maxKeywords][maxBodyLength - maxKeywordLen + 2];
component eqCheck[maxKeywords][maxBodyLength - maxKeywordLen + 1][maxKeywordLen];
// 1) Build the substring match signals
for (var i = 0; i < maxKeywords; i++) {
    for (var j = 0; j < (maxBodyLength - maxKeywordLen + 1); j++) {
        matchArr[i][j] <== 1;

        for (var kk = 0; kk < maxKeywordLen; kk++) {

                // eqCheck[i][j][kk] was declared above, so now we just wire it:
                eqCheck[i][j][kk] = IsEqual();
                eqCheck[i][j][kk].in[0] <== keywords[i][kk];
                eqCheck[i][j][kk].in[1] <== emailBody[j + kk];

                // Multiply matchArr[i][j] by eqCheck[i][j][kk].out
                matchArr[i][j] <== matchArr[i][j] * eqCheck[i][j][kk].out;
            }
    }
}

// 2) OR across each row matchArr[i][:] -> keywordFound[i].
signal notMatch;
signal notOrChain;

for (var i = 0; i < maxKeywords; i++) {
    // Initialize the OR chain for keyword i
    orChain[i][0] <== 0;

    for (var j = 0; j < (maxBodyLength - maxKeywordLen + 1); j++) {
        notOrChain <== 1 - orChain[i][j];


        notMatch <== 1 - matchArr[i][j];

        // orChain[i][j+1] = 1 - (notOrChain * notMatch)
        orChain[i][j+1] <== 1 - (notOrChain * notMatch);
    }

    // The final OR result:
    keywordFound[i] <== orChain[i][(maxBodyLength - maxKeywordLen + 1)];
}

// 3) If i >= numKeywords, treat keywordFound[i] as automatically "found".
signal iLess[maxKeywords];
component cLessThan[maxKeywords];
for (var i = 0; i < maxKeywords; i++) {
    cLessThan[i] = LessThan(32);
    cLessThan[i].in[0] <== i;
    cLessThan[i].in[1] <== numKeywords;
    iLess[i] <== cLessThan[i].out;
}

// 4) requiredFound[i] = (i < numKeywords) ? keywordFound[i] : 1
signal requiredFound[maxKeywords];
signal product;
signal not_iLess;
signal not_keywordFound;

for (var i = 0; i < maxKeywords; i++) {
    not_iLess <== 1 - iLess[i];

    not_keywordFound <== 1 - keywordFound[i];

    // product = (1 - not_iLess) * not_keywordFound
    product <== (1 - not_iLess) * not_keywordFound;

    // requiredFound[i] = 1 - product
    requiredFound[i] <== 1 - product;
}

// 5) allKeywordsPresent = AND of all requiredFound[i]
signal andChain[maxKeywords + 1];
andChain[0] <== 1;
for (var i = 0; i < maxKeywords; i++) {
    andChain[i+1] <== andChain[i] * requiredFound[i];
}
allKeywordsPresent <== andChain[maxKeywords];

}

//-------------------------------------
// Example instantiation
//-------------------------------------
component main = EmailVerifierWithKeywords(
    1024,   // maxHeadersLength
    10240,  // maxBodyLength
    121,    // n
    17,     // k
    0,      // ignoreBodyHashCheck
    1,      // enableHeaderMasking
    1,      // enableBodyMasking       1,      // removeSoftLineBreaks
    20,    // maxKeywords
    32      // maxKeywordLen
);
