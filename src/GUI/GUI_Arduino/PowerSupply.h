#ifndef POWER_SUPPLY_H
#define POWER_SUPPLY_H

#include "../lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ps_gui(void);
extern lv_obj_t* live_update_text;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
