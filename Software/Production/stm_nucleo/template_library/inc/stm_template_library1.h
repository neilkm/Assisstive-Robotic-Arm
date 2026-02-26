#ifndef STM_TEMPLATE_LIBRARY1_H
#define STM_TEMPLATE_LIBRARY1_H

/**
 * @brief Holds readiness state for the STM template library1 behavior.
 */
typedef struct {
    /** @brief Non-zero when ready, zero when not ready. */
    int ready;
} StmTemplateLibrary1;

/**
 * @brief Initializes an STM template library1 instance to not-ready.
 *
 * @param obj Pointer to the instance to initialize.
 */
void stm_template_library1_init(StmTemplateLibrary1 *obj);

/**
 * @brief Sets the ready state for an STM template library1 instance.
 *
 * @param obj Pointer to the instance to update.
 * @param ready Non-zero to mark ready, zero to mark not ready.
 */
void stm_template_library1_set_ready(StmTemplateLibrary1 *obj, int ready);

/**
 * @brief Returns whether an STM template library1 instance is ready.
 *
 * @param obj Pointer to the instance to query.
 * @return int 1 if ready, otherwise 0.
 */
int stm_template_library1_is_ready(const StmTemplateLibrary1 *obj);

#endif
