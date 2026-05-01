#include "esp_template_library.h"

/**
 * @brief Initializes an ESP template library instance.
 *
 * @param obj Pointer to the instance to initialize.
 */
void esp_template_library_init(EspTemplateLibrary *obj)
{
    obj->value = 0;
}

/**
 * @brief Sets the current value for an ESP template library instance.
 *
 * @param obj Pointer to the instance to update.
 * @param value Value to store.
 */
void esp_template_library_set(EspTemplateLibrary *obj, int value)
{
    obj->value = value;
}

/**
 * @brief Gets the current value from an ESP template library instance.
 *
 * @param obj Pointer to the instance to read.
 * @return int Current stored value.
 */
int esp_template_library_get(const EspTemplateLibrary *obj)
{
    return obj->value;
}
