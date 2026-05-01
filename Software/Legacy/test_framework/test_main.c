#include "test_framework.h"

#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

/**
 * @brief Represents a single registered test case.
 */
struct test_case {
	/** @brief Name of the suite containing this test. */
	const char *suite;
	/** @brief Test case name. */
	const char *name;
	/** @brief Function pointer to the test body. */
	test_fn_t fn;
};

#define MAX_TESTS 1024

static struct test_case tests[MAX_TESTS];
static int test_count = 0;

/**
 * @brief Registers a test case in the global test registry.
 *
 * @param suite Name of the test suite.
 * @param name Name of the test case.
 * @param fn Test function implementation.
 */
void test_framework_register(const char *suite, const char *name, test_fn_t fn)
{
	if (test_count >= MAX_TESTS) {
		fprintf(stderr, "Too many tests registered (max=%d)\n", MAX_TESTS);
		exit(1);
	}

	tests[test_count].suite = suite;
	tests[test_count].name = name;
	tests[test_count].fn = fn;
	test_count++;
}

/**
 * @brief Emits failure output for ASSERT_TRUE and terminates the process.
 *
 * @param expr Failing expression string.
 * @param file Source file of the assertion.
 * @param line Source line of the assertion.
 */
void test_framework_fail(const char *expr, const char *file, int line)
{
	fprintf(stderr, "  " COLOR_RED "[FAIL]" COLOR_RESET " ASSERT_TRUE failed at %s:%d (%s)\n", file, line, expr);
	exit(1);
}

/**
 * @brief Emits failure output for ASSERT_EQ_INT and terminates the process.
 *
 * @param expected Expected integer value.
 * @param actual Actual integer value.
 * @param file Source file of the assertion.
 * @param line Source line of the assertion.
 */
void test_framework_fail_eq_int(int expected, int actual, const char *file, int line)
{
	fprintf(stderr,
		"  " COLOR_RED "[FAIL]" COLOR_RESET " ASSERT_EQ_INT failed at %s:%d (expected=%d, actual=%d)\n",
		file, line, expected, actual);
	exit(1);
}

/**
 * @brief Runs every registered test case and prints standardized output.
 *
 * @return int Always 0 after executing all registered cases.
 */
int test_framework_run_all(void)
{
	int i;

	for (i = 0; i < test_count; i++) {
		printf("  [RUN] Case: [%s]\n", tests[i].name);
		tests[i].fn();
		printf("  " COLOR_GREEN "[OK ]" COLOR_RESET " Case: [%s]\n", tests[i].name);
	}

	printf("Ran %d test(s)\n", test_count);
	return 0;
}

/**
 * @brief Program entry point for C unit-test executables.
 *
 * @return int Exit status from the test framework runner.
 */
int main(void)
{
	return test_framework_run_all();
}
