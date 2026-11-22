/*
 * Test double for the SITL timer accessor.
 *
 * functions.c reads UTILITY_TIMER (-> sitl_tim_deref()) in its delay helpers.
 * The pure helpers under test never call it, but the linker still needs the symbol.
 */
#include "main.h"

SITL_TIM_TypeDef* sitl_tim_deref(int idx)
{
    static SITL_TIM_TypeDef fake_timer;
    (void)idx;
    return &fake_timer;
}
