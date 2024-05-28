//
// Created by Anders Cedronius on 2020-07-01.
//

#include <iostream>
#include "unit_test_1.h"
#include "unit_test_2.h"
#include "unit_test_3.h"
#include "unit_test_4.h"
#include "unit_test_5.h"
#include "unit_test_6.h"
#include "unit_test_7.h"
#include "unit_test_8.h"

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

    //Same as Unit Test 2 but is not embedding PCR
    std::cout << "Unit test4 started." << std::endl;
    UnitTest4 unitTest4;
    if (!unitTest4.runTest()) {
        std::cout << "Unit test 4 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Demultiplex a PES where the length is exactly 2 TS packets where the last TS packet is
    //able to carry all data meaning there is no need for a adaption field (stuffing)
    std::cout << "Unit test5 started." << std::endl;
    UnitTest5 unitTest5;
    if (!unitTest5.runTest()) {
        std::cout << "Unit test 5 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Check PTS/DTS/PCR values
    std::cout << "Unit test6 started." << std::endl;
    UnitTest6 unitTest6;
    if (!unitTest6.runTest()) {
        std::cout << "Unit test 6 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Check non 188-bytes TS packet demuxer feed
    std::cout << "Unit test7 started." << std::endl;
    UnitTest7 unitTest7;
    if (!unitTest7.runTest()) {
        std::cout << "Unit test 7 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Continuity counter tests
    std::cout << "Unit test8 started." << std::endl;
    UnitTest8 unitTest8;
    if (!unitTest8.runTest()) {
        std::cout << "Unit test 8 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    if (returnCode == EXIT_FAILURE) {
        std::cout << "Unit tests failed." << std::endl;
    } else {
        std::cout << "Unit tests pass." << std::endl;
    }

    return returnCode;
}

