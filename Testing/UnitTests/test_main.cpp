#include "test.h"

#ifndef TEST_DESCRIPTION
#define TEST_DESCRIPTION nullptr
#endif

int main(int argc, char** argv)
{
    return common_tests::run_pretty_gtest(argc, argv, TEST_DESCRIPTION);
}
