#ifndef POWER_SUPPLY_H
#define POWER_SUPPLY_H

#include "../lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ps_gui(void);
void ps_update_live_text_values(void);
void ps_set_values(float volts, float amps);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif