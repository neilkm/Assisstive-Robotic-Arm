#include "stm_template_library.h"

/**
 * @brief Initializes an STM template library instance.
 *
 * @param obj Pointer to the instance to initialize.
 */
void stm_template_library_init(StmTemplateLibrary *obj)
{
    obj->value = 0;
}

/**
 * @brief Sets the current value for an STM template library instance.
 *
 * @param obj Pointer to the instance to update.
 * @param value Value to store.
 */
void stm_template_library_set(StmTemplateLibrary *obj, int value)
{
    obj->value = value;
}

/**
 * @brief Gets the current value from an STM template library instance.
 *
 * @param obj Pointer to the instance to read.
 * @return int Current stored value.
 */
int stm_template_library_get(const StmTemplateLibrary *obj)
{
    return obj->value;
}
