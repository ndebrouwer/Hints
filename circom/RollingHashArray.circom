pragma circom 2.2.1;

include "PolyRollingHash.circom";

//-------------------------------------
// Rabin-Karp: Rolling hash array for scanning the entire body
//-------------------------------------
template RollingHashArray(
    maxBodyLength,
    maxKeywordLen,
    base
) {
    signal input emailBody[maxBodyLength];
    signal output rollingHash[maxBodyLength - maxKeywordLen + 1];

    // (1) Compute the hash for the first window [0..maxKeywordLen - 1].
    component firstHash = PolyRollingHash(maxKeywordLen, base);
    for (var i = 0; i < maxKeywordLen; i++) {
        //log"RollingHashArray: Assigning emailBody character ", i, " to firstHash");
        firstHash.str[i] <== emailBody[i];
        //log"firstHash.str[i]: ", firstHash.str[i]);
    }

    rollingHash[0] <== firstHash.hashVal;
    //log"RollingHashArray: Initial rolling hash is ", rollingHash[0]);

    // (2) We'll do rolling updates for j = 0..(loopSize - 1).
    var loopSize = maxBodyLength - maxKeywordLen;
    
    signal oldHash[loopSize];
    signal oldChar[loopSize];
    signal newChar[loopSize];
    signal subVal[loopSize];
    signal mulVal[loopSize];
    signal nextHash[loopSize];

    for (var j = 0; j < loopSize; j++) {
        oldHash[j] <== rollingHash[j];
        oldChar[j] <== emailBody[j];
        newChar[j] <== emailBody[j + maxKeywordLen];

        subVal[j] <== oldHash[j] 
                      - (oldChar[j] * firstHash.basePow[maxKeywordLen - 1]);

        mulVal[j] <== subVal[j] * firstHash.basePow[1];
        nextHash[j] <== mulVal[j] + newChar[j];

        rollingHash[j + 1] <== nextHash[j];
        //log"RollingHashArray: Rolling hash at index ", j + 1, " is ", rollingHash[j + 1]);
    }
}

//-------------------------------------
// Minimal test circuit as a template
//-------------------------------------
template TestRollingHashArray() {
    // We'll do a small example: maxBodyLength = 8, maxKeywordLen = 3
    signal input testBody[8];

    component rollingArr = RollingHashArray(8, 3, 256);
    for (var i = 0; i < 8; i++) {
        rollingArr.emailBody[i] <== testBody[i];
    }

    // The rollingHash output has length = (8 - 3 + 1) = 6
    // We can expose it for debugging or witness checks:
    signal output out[6];
    for (var j = 0; j < 6; j++) {
        out[j] <== rollingArr.rollingHash[j];
    }
}

// Finally, set the above test template as `main`.
//component main = TestRollingHashArray();
