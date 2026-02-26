#include "stm_template_library1.h"
#define TEST_SUITE_NAME "stm_template_library1_test_suite"
#include "test_framework.h"

/**
 * @test
 * @brief Verifies default not-ready state and ready transition for StmTemplateLibrary1.
 */
TEST(TemplateLibrary1TestInitializesNotReady)
{
    StmTemplateLibrary1 obj;
    stm_template_library1_init(&obj);
    ASSERT_EQ_INT(0, stm_template_library1_is_ready(&obj));
    stm_template_library1_set_ready(&obj, 1);
    ASSERT_EQ_INT(1, stm_template_library1_is_ready(&obj));
}
