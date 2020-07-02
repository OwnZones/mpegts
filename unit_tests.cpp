//
// Created by Anders Cedronius on 2020-07-01.
//

#include <iostream>
#include "unit_test_1.h"

int main(int argc, char *argv[]) {
    std::cout << "Running all unit tests." << std::endl;

    int returnCode = EXIT_SUCCESS;

    //mux/demux a vector increasing in size from 1 byte to 1000 bytes
    //Also run trough the combinations of PTS/DTS/PCR meaning PTS only and PTS+DTS + PCR
    UnitTest1 unitTest1;
    if (!unitTest1.runTest()) {
        std::cout << "Unit test 1 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    if (returnCode == EXIT_FAILURE) {
        std::cout << "Unit tests failed." << std::endl;
    } else {
        std::cout << "Unit tests pass." << std::endl;
    }

    return returnCode;
}

