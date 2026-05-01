#ifndef ESP_TEMPLATE_LIBRARY1_H
#define ESP_TEMPLATE_LIBRARY1_H

/**
 * @brief Holds enable-state for the ESP template library1 behavior.
 */
typedef struct {
    /** @brief Non-zero when enabled, zero when disabled. */
    int enabled;
} EspTemplateLibrary1;

/**
 * @brief Initializes an ESP template library1 instance to disabled.
 *
 * @param obj Pointer to the instance to initialize.
 */
void esp_template_library1_init(EspTemplateLibrary1 *obj);

/**
 * @brief Sets the enabled state for an ESP template library1 instance.
 *
 * @param obj Pointer to the instance to update.
 * @param enabled Non-zero to enable, zero to disable.
 */
void esp_template_library1_enable(EspTemplateLibrary1 *obj, int enabled);

/**
 * @brief Returns whether an ESP template library1 instance is enabled.
 *
 * @param obj Pointer to the instance to query.
 * @return int 1 if enabled, otherwise 0.
 */
int esp_template_library1_is_enabled(const EspTemplateLibrary1 *obj);

#endif
