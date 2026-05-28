#ifndef TESTING_UNIT_TESTS_TEST_H
#define TESTING_UNIT_TESTS_TEST_H

#include <cstdio>
#include <string>

#include <gtest/gtest.h>

namespace common_tests {

inline void print_separator(char fill, int width)
{
    for (int i = 0; i < width; ++i) {
        std::putchar(fill);
    }
    std::putchar('\n');
}

inline void print_centered_line(const std::string &text, int width)
{
    const int text_width = static_cast<int>(text.size());
    const int padding = (text_width < width) ? (width - text_width) / 2 : 0;

    std::printf("|");
    for (int i = 1; i < padding; ++i) {
        std::putchar(' ');
    }

    std::printf("%s", text.c_str());

    for (int i = padding + text_width; i < width - 1; ++i) {
        std::putchar(' ');
    }
    std::printf("|\n");
}

inline void print_test_harness_banner(const std::string &harness_name)
{
    constexpr int kBannerWidth = 72;

    std::putchar('\n');
    print_separator('=', kBannerWidth);
    print_centered_line("TEST HARNESS: " + harness_name, kBannerWidth);
    print_separator('=', kBannerWidth);
}

inline void print_test_case_banner(const std::string &test_case_name)
{
    constexpr int kBannerWidth = 72;

    std::putchar('\n');
    print_separator('-', kBannerWidth);
    print_centered_line("TEST CASE: " + test_case_name, kBannerWidth);
    print_separator('-', kBannerWidth);
}

inline void print_test_result(const std::string &test_case_name, bool passed)
{
    std::printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_case_name.c_str());
}

class PrettyTestPrinter final : public ::testing::EmptyTestEventListener {
public:
    void OnTestSuiteStart(const ::testing::TestSuite &test_suite) override
    {
        print_test_harness_banner(test_suite.name());
    }

    void OnTestStart(const ::testing::TestInfo &test_info) override
    {
        print_test_case_banner(test_info.name());
    }

    void OnTestEnd(const ::testing::TestInfo &test_info) override
    {
        print_test_result(test_info.name(), test_info.result()->Passed());
    }
};

inline void enable_pretty_gtest_output()
{
    ::testing::TestEventListeners &listeners =
        ::testing::UnitTest::GetInstance()->listeners();

    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new PrettyTestPrinter());
}

inline int run_pretty_gtest(int argc, char **argv, const char *description = nullptr)
{
    if (description && *description) {
        std::printf("  %s\n", description);
        std::fflush(stdout);
    }
    ::testing::InitGoogleTest(&argc, argv);
    enable_pretty_gtest_output();
    return RUN_ALL_TESTS();
}

}  // namespace common_tests

#endif
