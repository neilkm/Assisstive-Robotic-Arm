#include "stm_template_library1.h"

/**
 * @brief Initializes an STM template library1 instance.
 *
 * @param obj Pointer to the instance to initialize.
 */
void stm_template_library1_init(StmTemplateLibrary1 *obj)
{
    obj->ready = 0;
}

/**
 * @brief Updates the readiness state for an STM template library1 instance.
 *
 * @param obj Pointer to the instance to update.
 * @param ready Non-zero marks ready; zero marks not ready.
 */
void stm_template_library1_set_ready(StmTemplateLibrary1 *obj, int ready)
{
    obj->ready = ready ? 1 : 0;
}

/**
 * @brief Returns the readiness state for an STM template library1 instance.
 *
 * @param obj Pointer to the instance to query.
 * @return int 1 when ready, otherwise 0.
 */
int stm_template_library1_is_ready(const StmTemplateLibrary1 *obj)
{
    return obj->ready;
}
