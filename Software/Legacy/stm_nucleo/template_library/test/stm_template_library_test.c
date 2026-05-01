#include "stm_template_library.h"
#define TEST_SUITE_NAME "stm_template_library_test_suite"
#include "test_framework.h"

/**
 * @test
 * @brief Verifies initialization and setter/getter behavior for StmTemplateLibrary.
 */
TEST(TemplateLibraryTestInitializesToZero)
{
    StmTemplateLibrary obj;
    stm_template_library_init(&obj);
    ASSERT_EQ_INT(0, stm_template_library_get(&obj));
    stm_template_library_set(&obj, 7);
    ASSERT_EQ_INT(7, stm_template_library_get(&obj));
}
