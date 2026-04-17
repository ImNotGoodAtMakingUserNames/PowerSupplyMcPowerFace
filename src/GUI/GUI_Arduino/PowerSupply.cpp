#include "PowerSupply.h"
#include <stdio.h>

static lv_obj_t* text_voltage;
static lv_obj_t* text_current;

static float display_voltage = 0.0f;
static float display_current = 0.0f;

void ps_set_values(float volts, float amps)
{
    display_voltage = volts;
    display_current = amps;
}

static void ps_create_live_text_objects(void)
{
    text_voltage = lv_label_create(lv_screen_active());
    lv_label_set_text(text_voltage, "Voltage: 0.000 V");
    lv_obj_set_style_text_color(text_voltage, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_text_font(text_voltage, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(text_voltage, 10, 10);

    text_current = lv_label_create(lv_screen_active());
    lv_label_set_text(text_current, "Current: 0.000 A");
    lv_obj_set_style_text_color(text_current, lv_palette_darken(LV_PALETTE_GREEN, 3), 0);
    lv_obj_set_style_text_font(text_current, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(text_current, 10, 50);
}

void ps_update_live_text_values(void)
{
    static char buffer[64];

    snprintf(buffer, sizeof(buffer), "Voltage: %.3f V", display_voltage);
    lv_label_set_text(text_voltage, buffer);

    snprintf(buffer, sizeof(buffer), "Current: %.3f A", display_current);
    lv_label_set_text(text_current, buffer);
}

static void ps_update_timer_callback(lv_timer_t* timer)
{
    (void)timer;
    ps_update_live_text_values();
}

void ps_gui(void)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    ps_create_live_text_objects();
    lv_timer_create(ps_update_timer_callback, 500, NULL);
}