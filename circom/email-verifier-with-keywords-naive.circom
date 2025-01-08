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

//-------------------------------------
// Rabin-Karp: Polynomial rolling hash for a fixed substring length
//-------------------------------------
template PolyRollingHash(length, base) {
    // A simple polynomial hash: 
    //   hash = ( sum_{k=0..length-1}( str[k] * base^k ) ) mod p
    // where p is the SNARK prime in Circom (2^254 - something).
    //
    // Inputs:
    //   signal input str[length]
    // Outputs:
    //   signal output hashVal
    //   signal output basePow[length+1] // precomputed powers of base

    signal input str[length];
    signal output hashVal;
    signal output basePow[length + 1];

    // Precompute powers of base: basePow[k] = base^k
    basePow[0] <== 1;
    for (var i = 0; i < length; i++) {
        basePow[i + 1] <== basePow[i] * base;
    }

    // Compute the polynomial hash
    signal sumHash <== 0;
    for (var j = 0; j < length; j++) {
        signal partial <== str[j] * basePow[j];
        sumHash <== sumHash + partial;
    }

    hashVal <== sumHash;
}

//-------------------------------------
// Rabin-Karp: Rolling hash array for scanning the entire body
//-------------------------------------
template RollingHashArray(
    maxBodyLength,
    maxKeywordLen,
    base
) {
    // We produce rollingHash[j] = hash of the substring:
    //         emailBody[j .. j + maxKeywordLen - 1]
    // for j = 0..(maxBodyLength - maxKeywordLen)
    //
    // This is done in O(maxBodyLength) using the rolling formula:
    //   hash_{j+1} = ( (hash_j - emailBody[j]*base^(maxKeywordLen-1)) * base
    //                  + emailBody[j + maxKeywordLen] ) mod p

    signal input emailBody[maxBodyLength];
    signal output rollingHash[maxBodyLength - maxKeywordLen + 1];

    // 1) Compute the hash for the first window [0..maxKeywordLen - 1].
    component firstHash = PolyRollingHash(maxKeywordLen, base);
    for (var i = 0; i < maxKeywordLen; i++) {
        firstHash.str[i] <== emailBody[i];
    }
    rollingHash[0] <== firstHash.hashVal;

    // 2) Rolling updates for j=0..(maxBodyLength - maxKeywordLen - 1)
    for (var j = 0; j < (maxBodyLength - maxKeywordLen); j++) {
        signal oldHash <== rollingHash[j];
        signal oldChar <== emailBody[j];
        signal newChar <== emailBody[j + maxKeywordLen];

        // subVal = oldHash - oldChar * base^(maxKeywordLen - 1)
        signal subVal <== oldHash - (oldChar * firstHash.basePow[maxKeywordLen - 1]);

        // multiply by base
        signal mulVal <== subVal * firstHash.basePow[1];

        // add the newChar
        signal newHash <== mulVal + newChar;

        rollingHash[j + 1] <== newHash;
    }
}

//-------------------------------------
// EmailVerifierWithKeywords using Rabin-Karp for body substring checking
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
    maxKeywordLen
) {
    // 1) Declare inputs for EmailVerifier
    signal input emailHeader[maxHeadersLength];
    signal input emailHeaderLength;
    signal input pubkey[k];
    signal input signature[k];
    signal input bodyHashIndex;
    signal input precomputedSHA[32];
    signal input emailBody[maxBodyLength];
    signal input emailBodyLength;
    signal input decodedEmailBodyIn[maxBodyLength];
    signal input bodyMask[maxBodyLength];

    // 2) Outputs from EmailVerifier
    signal output pubkeyHash;
    signal output maskedHeader[maxHeadersLength];
    signal output maskedBody[maxBodyLength];

    // 3) Instantiate the underlying EmailVerifier circuit
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

    // Connect emailVerifier signals
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

    // 4) Add the keyword checks (Rabin-Karp method on the verified body)
    signal input numKeywords;
    signal input keywords[maxKeywords][maxKeywordLen];
    signal output allKeywordsPresent;

    // 4A) Compute hash for each keyword using polynomial rolling hash
    signal keywordHash[maxKeywords];
    var BASE = 256; // compile-time constant for base

    component kwHash[maxKeywords];
    for (var i = 0; i < maxKeywords; i++) {
        kwHash[i] = PolyRollingHash(maxKeywordLen, BASE);
        for (var c = 0; c < maxKeywordLen; c++) {
            kwHash[i].str[c] <== keywords[i][c];
        }
        keywordHash[i] <== kwHash[i].hashVal;
    }

    // 4B) Compute rolling hashes of emailBody for every substring of length maxKeywordLen
    component bodyRH = RollingHashArray(maxBodyLength, maxKeywordLen, BASE);
    for (var x = 0; x < maxBodyLength; x++) {
        bodyRH.emailBody[x] <== emailBody[x];
    }

    var numSubstrings = maxBodyLength - maxKeywordLen + 1;

    // 4C) Check if rollingHash matches any keyword hash.
    signal matchArr[maxKeywords][numSubstrings];
    component eqHash[maxKeywords][numSubstrings];

    for (var ki = 0; ki < maxKeywords; ki++) {
        for (var j = 0; j < numSubstrings; j++) {
            eqHash[ki][j] = IsEqual();
            eqHash[ki][j].in[0] <== bodyRH.rollingHash[j];
            eqHash[ki][j].in[1] <== keywordHash[ki];

            matchArr[ki][j] <== eqHash[ki][j].out;
        }
    }

    // (As in standard Rabin-Karp, you might do a fallback character-by-character check
    //  to guard against collisions, omitted here for brevity.)

    // 4D) OR-chain each row of matchArr[i][:] => keywordFound[i]
    signal orChain[maxKeywords][numSubstrings + 1];
    signal keywordFound[maxKeywords];

    signal notOrChain;
    signal notMatch;
    for (var i = 0; i < maxKeywords; i++) {
        orChain[i][0] <== 0;
        for (var j = 0; j < numSubstrings; j++) {
            notOrChain <== 1 - orChain[i][j];
            notMatch <== 1 - matchArr[i][j];
            orChain[i][j + 1] <== 1 - (notOrChain * notMatch);
        }
        keywordFound[i] <== orChain[i][numSubstrings];
    }

    // 4E) If i >= numKeywords, treat keywordFound[i] as automatically found
    signal iLess[maxKeywords];
    component cLessThan[maxKeywords];
    for (var i = 0; i < maxKeywords; i++) {
        cLessThan[i] = LessThan(32);
        cLessThan[i].in[0] <== i;
        cLessThan[i].in[1] <== numKeywords;
        iLess[i] <== cLessThan[i].out;
    }

    signal requiredFound[maxKeywords];
    signal product;
    signal not_iLess;
    signal not_keywordFound;
    for (var i = 0; i < maxKeywords; i++) {
        not_iLess <== 1 - iLess[i];
        not_keywordFound <== 1 - keywordFound[i];
        // product = ( (1 - not_iLess) * not_keywordFound )
        product <== (1 - not_iLess) * not_keywordFound;
        // requiredFound[i] = 1 - product
        requiredFound[i] <== 1 - product;
    }

    // 4F) allKeywordsPresent = AND of requiredFound[i]
    signal andChain[maxKeywords + 1];
    andChain[0] <== 1;
    for (var i = 0; i < maxKeywords; i++) {
        andChain[i + 1] <== andChain[i] * requiredFound[i];
    }
    allKeywordsPresent <== andChain[maxKeywords];
}

//-------------------------------------
// Example instantiation
//-------------------------------------
component main = EmailVerifierWithKeywords(
    1024,    // maxHeadersLength
    10240,   // maxBodyLength
    121,     // n
    17,      // k
    0,       // ignoreBodyHashCheck
    1,       // enableHeaderMasking
    1,       // enableBodyMasking
    1,       // removeSoftLineBreaks
    20,      // maxKeywords
    32       // maxKeywordLen
);
