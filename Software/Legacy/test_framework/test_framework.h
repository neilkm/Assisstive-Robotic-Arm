#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Function signature for a single registered unit test.
 */
typedef void (*test_fn_t)(void);

/**
 * @brief Registers a test function with a suite and case name.
 *
 * @param suite Name of the suite that owns the test.
 * @param name Name of the test case.
 * @param fn Function pointer for the test case body.
 */
void test_framework_register(const char *suite, const char *name, test_fn_t fn);

/**
 * @brief Runs all registered tests and prints per-case results.
 *
 * @return int Always returns 0 when execution reaches completion.
 */
int test_framework_run_all(void);

/**
 * @brief Reports a failed ASSERT_TRUE check and aborts execution.
 *
 * @param expr Assertion expression text.
 * @param file Source file where failure occurred.
 * @param line Source line where failure occurred.
 */
void test_framework_fail(const char *expr, const char *file, int line);

/**
 * @brief Reports a failed ASSERT_EQ_INT check and aborts execution.
 *
 * @param expected Expected integer value.
 * @param actual Actual integer value.
 * @param file Source file where failure occurred.
 * @param line Source line where failure occurred.
 */
void test_framework_fail_eq_int(int expected, int actual, const char *file, int line);

#ifndef TEST_SUITE_NAME
#define TEST_SUITE_NAME __FILE__
#endif

/**
 * @brief Declares and auto-registers a test case for the current suite.
 */
#define TEST(name) \
	static void name(void); \
	__attribute__((constructor)) static void register_##name(void) \
	{ \
		test_framework_register(TEST_SUITE_NAME, #name, name); \
	} \
	static void name(void)

/**
 * @brief Asserts that an expression is true.
 */
#define ASSERT_TRUE(expr) \
	do { \
		if (!(expr)) { \
			test_framework_fail(#expr, __FILE__, __LINE__); \
		} \
	} while (0)

/**
 * @brief Asserts that two integer expressions are equal.
 */
#define ASSERT_EQ_INT(expected, actual) \
	do { \
		int _expected = (expected); \
		int _actual = (actual); \
		if (_expected != _actual) { \
			test_framework_fail_eq_int(_expected, _actual, __FILE__, __LINE__); \
		} \
	} while (0)

#endif
