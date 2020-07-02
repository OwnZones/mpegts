//
// Created by Anders Cedronius on 2020-07-01.
//

#include <iostream>
#include "unit_test_1.h"
#include "unit_test_2.h"
#include "unit_test_3.h"

int main(int argc, char *argv[]) {
    std::cout << "Running all unit tests." << std::endl;

    int returnCode = EXIT_SUCCESS;

    //mux/demux a vector increasing in size from 1 byte to a set size inside the unit test
    //please read the comment inside the unit test for more information

    std::cout << "Unit test1 started." << std::endl;
    UnitTest1 unitTest1;
    if (!unitTest1.runTest()) {
        std::cout << "Unit test 1 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }


    //mux/demux a vector increasing in size from 1 byte to a set size inside the unit test
    // Difference is that the demuxer is fed multiple ts packets to parse
    //please read the comment inside the unit test for more information

    std::cout << "Unit test2 started." << std::endl;
    UnitTest2 unitTest2;
    if (!unitTest2.runTest()) {
        std::cout << "Unit test 2 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }


    //Same as Unit Test 1 but is not embedding PCR
    std::cout << "Unit test3 started." << std::endl;
    UnitTest3 unitTest3;
    if (!unitTest3.runTest()) {
        std::cout << "Unit test 3 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    if (returnCode == EXIT_FAILURE) {
        std::cout << "Unit tests failed." << std::endl;
    } else {
        std::cout << "Unit tests pass." << std::endl;
    }

    return returnCode;
}

