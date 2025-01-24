pragma circom 2.2.1;

// Make sure the path to RabinKarpKeywords.circom is correct in your environment
include "RabinKarp.circom";

/*
  A small test harness that:
    - Takes up to 10 chars in the body
    - Looks for up to 1 keyword of length 3
    - Then outputs whether we found that keyword
*/

template RabinKarpTest() {
    signal input inputBody[10];       // For this demo, body up to length 10
    signal input inputKeywords[1][3]; // For this demo, only 1 keyword, length 3
    signal input numKeywords;         // Typically 1 if we want to search for 1 keyword

    // Instantiate the RabinKarpKeywords template
    component test = RabinKarpKeywords(
        10,   // maxBodyLength
        3,    // maxKeywordLen
        1,    // maxKeywords
        32,   // bitwidth for LessThan
        256   // BASE for rolling hash
    );

    // Connect inputs
    for (var i = 0; i < 10; i++) {
        test.emailBody[i] <== inputBody[i];
    }
    for (var k = 0; k < 1; k++) {
        for (var c = 0; c < 3; c++) {
            test.keywords[k][c] <== inputKeywords[k][c];
        }
    }
    test.numKeywords <== numKeywords;

    // Expose the result of the search
    signal output result;
    result <== test.allKeywordsPresent;
}

// The main component to compile
component main = RabinKarpTest();
