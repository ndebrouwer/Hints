// ------------------------------------------------------------
// (2) New RabinKarpKeywords template replicating the above logic
// ------------------------------------------------------------
pragma circom 2.2.1;

include "./PolyRollingHash.circom";
include "./RollingHashArray.circom";
include "../node_modules/circomlib/circuits/comparators.circom";
include "../node_modules/circomlib/circuits/mux1.circom"; // For IsEqual, LessThan, etc.

template RabinKarpKeywords(
    maxBodyLength,
    maxKeywordLen,
    maxKeywords,
    bitwidth,     // for LessThan
    BASE
) {
    // Inputs
    signal input emailBody[maxBodyLength];
    signal input keywords[maxKeywords][maxKeywordLen];
    signal input numKeywords;

    // Output
    signal output allKeywordsPresent;

    // 4A) Compute hash for each keyword using polynomial rolling hash
    signal keywordHash[maxKeywords];

    component kwHash[maxKeywords];
    for (var i = 0; i < maxKeywords; i++) {
        kwHash[i] = PolyRollingHash(maxKeywordLen, BASE);
        for (var c = 0; c < maxKeywordLen; c++) {
            kwHash[i].str[c] <== keywords[i][c];
        }
        keywordHash[i] <== kwHash[i].hashVal;
        //log"RabinKarpKeywords: Keyword ", i, " hash is ", keywordHash[i]); 
    }

    // 4B) Compute rolling hashes of emailBody for every substring of length maxKeywordLen
    component bodyRH = RollingHashArray(maxBodyLength, maxKeywordLen, BASE);
    for (var x = 0; x < maxBodyLength; x++) {
        bodyRH.emailBody[x] <== emailBody[x];
    }
    //log"RabinKarpKeywords: Completed rolling hash computation for body."); 
    var numSubstrings = maxBodyLength - maxKeywordLen + 1;

    // 4C) Compare rolling hashes to keyword hashes
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

    // 4D) OR-chain each row => keywordFound[i]
    signal orChain[maxKeywords][numSubstrings + 1];
    signal keywordFound[maxKeywords];
    signal notOrChain[maxKeywords][numSubstrings];
    signal notMatch[maxKeywords][numSubstrings];

    for (var i = 0; i < maxKeywords; i++) {
        orChain[i][0] <== 0;
        for (var j = 0; j < numSubstrings; j++) {
            notOrChain[i][j] <== 1 - orChain[i][j];
            notMatch[i][j] <== 1 - matchArr[i][j];
            orChain[i][j + 1] <== 1 - (notOrChain[i][j] * notMatch[i][j]);
        }
        keywordFound[i] <== orChain[i][numSubstrings];
    }
    //log"RabinKarpKeywords: finished OR chaining");

    // 4E) If i >= numKeywords, treat keywordFound[i] as automatically found
    signal iLess[maxKeywords];
    component cLessThan[maxKeywords];
    for (var i = 0; i < maxKeywords; i++) {
        cLessThan[i] = LessThan(bitwidth);
        cLessThan[i].in[0] <== i;
        cLessThan[i].in[1] <== numKeywords;
        iLess[i] <== cLessThan[i].out;
    }

    signal requiredFound[maxKeywords];
    signal not_iLess[maxKeywords];
    signal not_keywordFound[maxKeywords];
    signal product[maxKeywords];

    for (var i = 0; i < maxKeywords; i++) {
        not_iLess[i] <== 1 - iLess[i];
        not_keywordFound[i] <== 1 - keywordFound[i];
        product[i] <== (1 - not_iLess[i]) * not_keywordFound[i];
        requiredFound[i] <== 1 - product[i];
    }
    //log"RabinKarpKeywords: finished keyword found logic");

    // 4F) allKeywordsPresent = AND of requiredFound[i]
    signal andChain[maxKeywords + 1];
    andChain[0] <== 1;
    for (var i = 0; i < maxKeywords; i++) {
        andChain[i + 1] <== andChain[i] * requiredFound[i];
    }
    allKeywordsPresent <== andChain[maxKeywords];
}
