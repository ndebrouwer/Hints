pragma circom 2.2.1;

//-------------------------------------
// Rabin-Karp: Polynomial rolling hash for a fixed substring length
//-------------------------------------
template PolyRollingHash(length, base) {
    signal input str[length];
    signal output hashVal;
    signal output basePow[length + 1];
 
    // Precompute powers of base
    basePow[0] <== 1;
    for (var i = 0; i < length; i++) {
        //logbasePow[0]);
        //log"PolyRollingHash: Computing basePow for index ", i); 
        basePow[i + 1] <== basePow[i] * base;
        //logbasePow[0]);
        //log"PolyRollingHash: basePow is ", basePow[i]); 
        //log"base is ", base);

    }

    // sum up all partial products
    signal sumHash;

    signal partial[length]; // declare array once
    // Declare a temporary signal array for summing intermediate values.
    signal tempSum[length + 1]; 
    tempSum[0] <== 0; // Initialize the first element to 0.

    for (var j = 0; j < length; j++) {
        partial[j] <== str[j] * basePow[j];
        tempSum[j + 1] <== tempSum[j] + partial[j]; // Update the sum in the temporary array.
        //log"PolyRollingHash: Partial sum at index ", j, " is ", tempSum[j + 1]); 
    }

    sumHash <== tempSum[length]; // Assign the final sum to sumHash.
    hashVal <== sumHash;
}

// Minimal test circuit as a template
template TestCircuit() {
    signal input testStr[4];

    component polyHash = PolyRollingHash(4, 256);
    for (var i = 0; i < 4; i++) {
        polyHash.str[i] <== testStr[i];
    }

    // If you want to expose the hash for debugging:
    signal output out;
    out <== polyHash.hashVal;
}

// Then set this template as `main`:
//component main = TestCircuit();