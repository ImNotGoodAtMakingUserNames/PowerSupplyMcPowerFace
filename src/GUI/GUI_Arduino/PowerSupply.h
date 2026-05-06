#ifndef POWER_SUPPLY_H
#define POWER_SUPPLY_H

#include "../lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

const char* supervisor_last_fault();

extern bool sup_fault_12v_uv, sup_fault_12v_ov, sup_fault_12v_oc;
extern bool sup_fault_5v_uv,  sup_fault_5v_ov,  sup_fault_5v_oc;
extern bool sup_fault_3v3_uv, sup_fault_3v3_ov, sup_fault_3v3_oc;

typedef enum {
    PSU_STATE_STANDBY,
    PSU_STATE_ENABLING,
    PSU_STATE_RUNNING,
    PSU_STATE_FAULT,
} psu_state_t;

void ps_gui(void);
extern lv_obj_t* live_update_text;
void ps_set_values(float v12v, float v12a,
                   float v5v, float v5a,
                   float v33v, float v33a,
                   float ac_w, float dc_w,
                   float eff, float temp_c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif