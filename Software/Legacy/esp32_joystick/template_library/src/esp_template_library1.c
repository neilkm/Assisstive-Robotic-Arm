#include "esp_template_library1.h"

/**
 * @brief Initializes an ESP template library1 instance.
 *
 * @param obj Pointer to the instance to initialize.
 */
void esp_template_library1_init(EspTemplateLibrary1 *obj)
{
    obj->enabled = 0;
}

/**
 * @brief Updates the enabled state for an ESP template library1 instance.
 *
 * @param obj Pointer to the instance to update.
 * @param enabled Non-zero enables the instance; zero disables it.
 */
void esp_template_library1_enable(EspTemplateLibrary1 *obj, int enabled)
{
    obj->enabled = enabled ? 1 : 0;
}

/**
 * @brief Returns the enabled state for an ESP template library1 instance.
 *
 * @param obj Pointer to the instance to query.
 * @return int 1 when enabled, otherwise 0.
 */
int esp_template_library1_is_enabled(const EspTemplateLibrary1 *obj)
{
    return obj->enabled;
}
