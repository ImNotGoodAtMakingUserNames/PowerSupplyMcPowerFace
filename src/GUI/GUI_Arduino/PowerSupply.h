#ifndef POWER_SUPPLY_H
#define POWER_SUPPLY_H

#include "../lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ps_gui(void);
extern lv_obj_t* live_update_text;
void ps_set_values(float v12v, float v12a,
                   float v5v,  float v5a,
                   float v33v, float v33a,
                   float ac_w, float temp_c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif