#include "esp_template_library.h"
#define TEST_SUITE_NAME "esp_template_library_test_suite"
#include "test_framework.h"

/**
 * @test
 * @brief Verifies initialization and setter/getter behavior for EspTemplateLibrary.
 */
TEST(TemplateLibraryTestInitializesToZero)
{
    EspTemplateLibrary obj;
    esp_template_library_init(&obj);
    ASSERT_EQ_INT(0, esp_template_library_get(&obj));
    esp_template_library_set(&obj, 5);
    ASSERT_EQ_INT(5, esp_template_library_get(&obj));
}
