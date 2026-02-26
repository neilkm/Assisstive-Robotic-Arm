#ifndef ESP_TEMPLATE_LIBRARY_H
#define ESP_TEMPLATE_LIBRARY_H

/**
 * @brief Holds integer state for the ESP template library.
 */
typedef struct {
    /** @brief Stored integer value. */
    int value;
} EspTemplateLibrary;

/**
 * @brief Initializes an ESP template library instance to its default state.
 *
 * @param obj Pointer to the instance to initialize.
 */
void esp_template_library_init(EspTemplateLibrary *obj);

/**
 * @brief Sets the stored value of an ESP template library instance.
 *
 * @param obj Pointer to the instance to update.
 * @param value New value to store.
 */
void esp_template_library_set(EspTemplateLibrary *obj, int value);

/**
 * @brief Returns the stored value from an ESP template library instance.
 *
 * @param obj Pointer to the instance to read.
 * @return int Current stored value.
 */
int esp_template_library_get(const EspTemplateLibrary *obj);

#endif
