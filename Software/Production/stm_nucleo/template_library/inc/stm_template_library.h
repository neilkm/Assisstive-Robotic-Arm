#ifndef STM_TEMPLATE_LIBRARY_H
#define STM_TEMPLATE_LIBRARY_H

/**
 * @brief Holds integer state for the STM template library.
 */
typedef struct {
    /** @brief Stored integer value. */
    int value;
} StmTemplateLibrary;

/**
 * @brief Initializes an STM template library instance to its default state.
 *
 * @param obj Pointer to the instance to initialize.
 */
void stm_template_library_init(StmTemplateLibrary *obj);

/**
 * @brief Sets the stored value of an STM template library instance.
 *
 * @param obj Pointer to the instance to update.
 * @param value New value to store.
 */
void stm_template_library_set(StmTemplateLibrary *obj, int value);

/**
 * @brief Returns the stored value from an STM template library instance.
 *
 * @param obj Pointer to the instance to read.
 * @return int Current stored value.
 */
int stm_template_library_get(const StmTemplateLibrary *obj);

#endif
