    pragma circom 2.2.1;

    include "./email-verifier-with-keywords.circom";

    // Example instantiation with your parameters
    component main = EmailVerifierWithKeywords(
        512,   // maxHeadersLength
        64,    // maxBodyLength
        121,   // n
        17,    // k
        0,     // ignoreBodyHashCheck
        0,     // enableHeaderMasking
        0,     // enableBodyMasking
        1,     // removeSoftLineBreaks
        1,     // maxKeywords
        16,    // maxKeywordLen
        32,    // bitPerChunk
        17     // chunkSize
    );
