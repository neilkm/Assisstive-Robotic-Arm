#include "esp_template_library1.h"
#define TEST_SUITE_NAME "esp_template_library1_test_suite"
#include "test_framework.h"

/**
 * @test
 * @brief Verifies default disabled state and enable transition for EspTemplateLibrary1.
 */
TEST(TemplateLibrary1TestInitializesDisabled)
{
    EspTemplateLibrary1 obj;
    esp_template_library1_init(&obj);
    ASSERT_EQ_INT(0, esp_template_library1_is_enabled(&obj));
    esp_template_library1_enable(&obj, 1);
    ASSERT_EQ_INT(1, esp_template_library1_is_enabled(&obj));
}
