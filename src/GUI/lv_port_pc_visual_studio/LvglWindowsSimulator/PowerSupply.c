#include "PowerSupply.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define VERT_RES 272
#define HORI_RES 480

/* -----------------------------------------------------------------------------
 * Simulator stubs
 * -----------------------------------------------------------------------------
 * On real hardware, supervisor_last_fault() lives in the supervisor code that
 * ships with the .ino sketch. The simulator doesn't have it, so we stub it
 * here. Wrap in #ifdef SIMULATOR_BUILD (and pass -DSIMULATOR_BUILD to your
 * sim compiler) if/when you put this file back into a hardware build.
 * -------------------------------------------------------------------------- */
const char* supervisor_last_fault(void)
{
    return "None";
}

/* UI Objects Structure */
typedef struct {
    /* Temperature Scale */
    lv_obj_t* sys_temp_scale;

    /* Scale dimensions and positioning */
    int32_t scale_w;
    int32_t scale_h;
    int32_t scale_spacing;
    int32_t right_offset;

    /* Voltage Scales */
    lv_obj_t* v12_scale;
    lv_obj_t* v12_needle;
    lv_obj_t* text_v12_label;
    lv_obj_t* text_v12_current;
    int32_t v12_needle_width;

    lv_obj_t* v5_scale;
    lv_obj_t* v5_needle;
    lv_obj_t* text_v5_label;
    lv_obj_t* text_v5_current;
    int32_t v5_needle_width;

    lv_obj_t* v33_scale;
    lv_obj_t* v33_needle;
    lv_obj_t* text_v33_label;
    lv_obj_t* text_v33_current;
    int32_t v33_needle_width;

    /* Text labels for live updates */
    lv_obj_t* live_update_text;
    lv_obj_t* text_value_1;
    lv_obj_t* text_value_2;
    lv_obj_t* text_value_3;

    /* Power rail text displays */
    lv_obj_t* text_12v_rail;
    lv_obj_t* text_5v_rail;
    lv_obj_t* text_33v_rail;
    lv_obj_t* text_ac_power;
    lv_obj_t* text_temperature;
    lv_obj_t* text_last_fault;

    /* Left side metrics */
    lv_obj_t* text_wall_power;
    lv_obj_t* text_system_power;
    lv_obj_t* text_efficiency;

    /* Status indicator circle */
    lv_obj_t* status_circle;
    bool circle_is_red;

    lv_obj_t* screen;
} ps_ui_objects_t;

/* Global UI objects */
static ps_ui_objects_t ui_objects = { 0 };

/* Global variables for power rail values (initialized to plausible example values) */
static float v12_rail_voltage = 12.0f;
static float v12_rail_current = 1.5f;
static float v5_rail_voltage = 5.0f;
static float v5_rail_current = 0.8f;
static float v33_rail_voltage = 3.3f;
static float v33_rail_current = 0.5f;
static float ac_power_input = 50.0f;
static float dc_power_output = 45.0f;
static float sys_efficiency = 90.0f;
static float system_temperature = 25.0f;

static void ps_toggle_status_circle(void)
{
    if (ui_objects.status_circle) {
        if (ui_objects.circle_is_red) {
            lv_obj_set_style_bg_color(ui_objects.status_circle, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        }
        else {
            lv_obj_set_style_bg_color(ui_objects.status_circle, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        }
        ui_objects.circle_is_red = !ui_objects.circle_is_red;
    }
}

static void ps_create_status_circle(void)
{
    ui_objects.status_circle = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui_objects.status_circle, 20, 20);
    lv_obj_set_style_radius(ui_objects.status_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_objects.status_circle, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_objects.status_circle, 0, LV_PART_MAIN);
    lv_obj_set_pos(ui_objects.status_circle, 10, 10);
    ui_objects.circle_is_red = true;
}

static void V33_set_needle(lv_obj_t* needle, double volt_val, int32_t scale_h, int32_t scale_w, int32_t scale_x_loc, int32_t scale_y_loc, double range_min, double range_max, int32_t needle_w)
{
    if (volt_val < range_min) volt_val = range_min;
    if (volt_val > range_max) volt_val = range_max;

    int32_t x_start = scale_x_loc + scale_w;
    int32_t y_loc = (int32_t)(scale_y_loc + (double)scale_h * (range_max - volt_val) / (range_max - range_min));

    static lv_point_precise_t pts[2];
    pts[0].x = x_start;
    pts[0].y = y_loc;
    pts[1].x = x_start + needle_w;
    pts[1].y = y_loc;

    lv_line_set_points(needle, pts, 2);
}

static void V5_set_needle(lv_obj_t* needle, double volt_val, int32_t scale_h, int32_t scale_w, int32_t scale_x_loc, int32_t scale_y_loc, double range_min, double range_max, int32_t needle_w)
{
    if (volt_val < range_min) volt_val = range_min;
    if (volt_val > range_max) volt_val = range_max;

    int32_t x_start = scale_x_loc + scale_w;
    int32_t y_loc = (int32_t)(scale_y_loc + (double)scale_h * (range_max - volt_val) / (range_max - range_min));

    static lv_point_precise_t pts[2];
    pts[0].x = x_start;
    pts[0].y = y_loc;
    pts[1].x = x_start + needle_w;
    pts[1].y = y_loc;

    lv_line_set_points(needle, pts, 2);
}

static void V12_set_needle(lv_obj_t* needle, double volt_val, int32_t scale_h, int32_t scale_w, int32_t scale_x_loc, int32_t scale_y_loc, double range_min, double range_max, int32_t needle_w)
{
    if (volt_val < range_min) volt_val = range_min;
    if (volt_val > range_max) volt_val = range_max;

    int32_t x_start = scale_x_loc + scale_w;
    int32_t y_loc = (int32_t)(scale_y_loc + (double)scale_h * (range_max - volt_val) / (range_max - range_min));

    static lv_point_precise_t pts[2];
    pts[0].x = x_start;
    pts[0].y = y_loc;
    pts[1].x = x_start + needle_w;
    pts[1].y = y_loc;

    lv_line_set_points(needle, pts, 2);
}

static void ps_update_v33_needle(void)
{
    static char buffer[32];

    int32_t v33_x_loc = HORI_RES / 2 - ui_objects.scale_w / 2 + ui_objects.right_offset;
    int32_t v33_y_loc = VERT_RES / 2 - ui_objects.scale_h / 2;

    V33_set_needle(ui_objects.v33_needle, v33_rail_voltage, ui_objects.scale_h, ui_objects.scale_w,
        v33_x_loc, v33_y_loc, 3.10, 3.50, ui_objects.v33_needle_width);

    snprintf(buffer, sizeof(buffer), "%.2f A", v33_rail_current);
    lv_label_set_text(ui_objects.text_v33_current, buffer);
}

static void ps_update_v5_needle(void)
{
    static char buffer[32];

    int32_t v5_x_loc = (HORI_RES / 2 - ui_objects.scale_w / 2 + ui_objects.right_offset) - ui_objects.scale_spacing;
    int32_t v5_y_loc = VERT_RES / 2 - ui_objects.scale_h / 2;

    V5_set_needle(ui_objects.v5_needle, v5_rail_voltage, ui_objects.scale_h, ui_objects.scale_w,
        v5_x_loc, v5_y_loc, 4.65, 5.35, ui_objects.v5_needle_width);

    snprintf(buffer, sizeof(buffer), "%.2f A", v5_rail_current);
    lv_label_set_text(ui_objects.text_v5_current, buffer);
}

static void ps_update_v12_needle(void)
{
    static char buffer[32];

    int32_t v12_x_loc = (HORI_RES / 2 - ui_objects.scale_w / 2 + ui_objects.right_offset) - (ui_objects.scale_spacing * 2);
    int32_t v12_y_loc = VERT_RES / 2 - ui_objects.scale_h / 2;

    V12_set_needle(ui_objects.v12_needle, v12_rail_voltage, ui_objects.scale_h, ui_objects.scale_w,
        v12_x_loc, v12_y_loc, 11.2, 12.8, ui_objects.v12_needle_width);

    snprintf(buffer, sizeof(buffer), "%.2f A", v12_rail_current);
    lv_label_set_text(ui_objects.text_v12_current, buffer);
}

static void ps_create_voltage_scale_v33(int32_t x_loc, int32_t y_loc, int32_t scale_w, int32_t scale_h, int32_t needle_w)
{
    ui_objects.v33_scale = lv_scale_create(lv_screen_active());
    lv_obj_set_size(ui_objects.v33_scale, scale_w, scale_h);
    lv_scale_set_label_show(ui_objects.v33_scale, true);
    lv_scale_set_mode(ui_objects.v33_scale, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_obj_set_x(ui_objects.v33_scale, x_loc);
    lv_obj_set_y(ui_objects.v33_scale, y_loc);

    lv_scale_set_total_tick_count(ui_objects.v33_scale, 41);
    lv_scale_set_major_tick_every(ui_objects.v33_scale, 10);

    lv_obj_set_style_length(ui_objects.v33_scale, 10, LV_PART_INDICATOR);
    lv_obj_set_style_length(ui_objects.v33_scale, 5, LV_PART_ITEMS);

    lv_scale_set_range(ui_objects.v33_scale, 310, 350);

    static const char* delta_labels[] = { "3.10 ", "3.20 ", "3.30 ", "3.40 ", "3.50 ", NULL };
    lv_scale_set_text_src(ui_objects.v33_scale, delta_labels);

    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);
    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&indicator_style, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_line_color(&indicator_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_width(&indicator_style, 10U);
    lv_style_set_line_width(&indicator_style, 2U);
    lv_obj_add_style(ui_objects.v33_scale, &indicator_style, LV_PART_INDICATOR);

    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    lv_style_set_line_color(&minor_ticks_style, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_width(&minor_ticks_style, 5U);
    lv_style_set_line_width(&minor_ticks_style, 2U);
    lv_obj_add_style(ui_objects.v33_scale, &minor_ticks_style, LV_PART_ITEMS);

    /* Color-coded tolerance sections */
    static lv_style_t red_indicator_style, red_ticks_style;
    static lv_style_t orange_indicator_style, orange_ticks_style;
    static lv_style_t green_indicator_style, green_ticks_style;

    lv_style_init(&red_indicator_style);
    lv_style_set_line_color(&red_indicator_style, lv_palette_darken(LV_PALETTE_RED, 2));
    lv_style_set_width(&red_indicator_style, 10U);
    lv_style_set_line_width(&red_indicator_style, 2U);

    lv_style_init(&red_ticks_style);
    lv_style_set_line_color(&red_ticks_style, lv_palette_darken(LV_PALETTE_RED, 2));
    lv_style_set_width(&red_ticks_style, 5U);
    lv_style_set_line_width(&red_ticks_style, 2U);

    lv_style_init(&orange_indicator_style);
    lv_style_set_line_color(&orange_indicator_style, lv_palette_darken(LV_PALETTE_ORANGE, 2));
    lv_style_set_width(&orange_indicator_style, 10U);
    lv_style_set_line_width(&orange_indicator_style, 2U);

    lv_style_init(&orange_ticks_style);
    lv_style_set_line_color(&orange_ticks_style, lv_palette_darken(LV_PALETTE_ORANGE, 2));
    lv_style_set_width(&orange_ticks_style, 5U);
    lv_style_set_line_width(&orange_ticks_style, 2U);

    lv_style_init(&green_indicator_style);
    lv_style_set_line_color(&green_indicator_style, lv_palette_darken(LV_PALETTE_GREEN, 2));
    lv_style_set_width(&green_indicator_style, 10U);
    lv_style_set_line_width(&green_indicator_style, 2U);

    lv_style_init(&green_ticks_style);
    lv_style_set_line_color(&green_ticks_style, lv_palette_darken(LV_PALETTE_GREEN, 2));
    lv_style_set_width(&green_ticks_style, 5U);
    lv_style_set_line_width(&green_ticks_style, 2U);

    lv_scale_section_t* section = lv_scale_add_section(ui_objects.v33_scale);
    lv_scale_set_section_range(ui_objects.v33_scale, section, 310, 313);
    lv_scale_set_section_style_indicator(ui_objects.v33_scale, section, &red_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v33_scale, section, &red_ticks_style);

    section = lv_scale_add_section(ui_objects.v33_scale);
    lv_scale_set_section_range(ui_objects.v33_scale, section, 313, 322);
    lv_scale_set_section_style_indicator(ui_objects.v33_scale, section, &orange_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v33_scale, section, &orange_ticks_style);

    section = lv_scale_add_section(ui_objects.v33_scale);
    lv_scale_set_section_range(ui_objects.v33_scale, section, 322, 337);
    lv_scale_set_section_style_indicator(ui_objects.v33_scale, section, &green_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v33_scale, section, &green_ticks_style);

    section = lv_scale_add_section(ui_objects.v33_scale);
    lv_scale_set_section_range(ui_objects.v33_scale, section, 337, 346);
    lv_scale_set_section_style_indicator(ui_objects.v33_scale, section, &orange_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v33_scale, section, &orange_ticks_style);

    section = lv_scale_add_section(ui_objects.v33_scale);
    lv_scale_set_section_range(ui_objects.v33_scale, section, 346, 350);
    lv_scale_set_section_style_indicator(ui_objects.v33_scale, section, &red_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v33_scale, section, &red_ticks_style);

    ui_objects.v33_needle = lv_line_create(lv_screen_active());
    lv_obj_set_style_line_color(ui_objects.v33_needle, lv_palette_darken(LV_PALETTE_PURPLE, 1), LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_objects.v33_needle, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ui_objects.v33_needle, true, LV_PART_MAIN);

    ui_objects.text_v33_label = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_v33_label, "3.3V");
    lv_obj_set_style_text_color(ui_objects.text_v33_label, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_font(ui_objects.text_v33_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(ui_objects.text_v33_label, x_loc + 15, y_loc - 30);

    ui_objects.text_v33_current = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_v33_current, "0.00 A");
    lv_obj_set_style_text_color(ui_objects.text_v33_current, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_font(ui_objects.text_v33_current, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(ui_objects.text_v33_current, x_loc + 15, y_loc + scale_h + 10);

    ui_objects.v33_needle_width = needle_w;
    V33_set_needle(ui_objects.v33_needle, 3.30, scale_h, scale_w, x_loc, y_loc, 3.10, 3.50, needle_w);
}

static void ps_create_voltage_scale_v5(int32_t x_loc, int32_t y_loc, int32_t scale_w, int32_t scale_h, int32_t needle_w)
{
    ui_objects.v5_scale = lv_scale_create(lv_screen_active());
    lv_obj_set_size(ui_objects.v5_scale, scale_w, scale_h);
    lv_scale_set_label_show(ui_objects.v5_scale, true);
    lv_scale_set_mode(ui_objects.v5_scale, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_obj_set_x(ui_objects.v5_scale, x_loc);
    lv_obj_set_y(ui_objects.v5_scale, y_loc);

    lv_scale_set_total_tick_count(ui_objects.v5_scale, 41);
    lv_scale_set_major_tick_every(ui_objects.v5_scale, 10);

    lv_obj_set_style_length(ui_objects.v5_scale, 10, LV_PART_INDICATOR);
    lv_obj_set_style_length(ui_objects.v5_scale, 5, LV_PART_ITEMS);

    lv_scale_set_range(ui_objects.v5_scale, 460, 540);

    static const char* v5_labels[] = { "4.60", "4.80", "5.00", "5.20", "5.40", NULL };
    lv_scale_set_text_src(ui_objects.v5_scale, v5_labels);

    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);
    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&indicator_style, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_line_color(&indicator_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_width(&indicator_style, 10U);
    lv_style_set_line_width(&indicator_style, 2U);
    lv_obj_add_style(ui_objects.v5_scale, &indicator_style, LV_PART_INDICATOR);

    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    lv_style_set_line_color(&minor_ticks_style, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_width(&minor_ticks_style, 5U);
    lv_style_set_line_width(&minor_ticks_style, 2U);
    lv_obj_add_style(ui_objects.v5_scale, &minor_ticks_style, LV_PART_ITEMS);

    static lv_style_t red_indicator_style, red_ticks_style;
    static lv_style_t orange_indicator_style, orange_ticks_style;
    static lv_style_t green_indicator_style, green_ticks_style;

    lv_style_init(&red_indicator_style);
    lv_style_set_line_color(&red_indicator_style, lv_palette_darken(LV_PALETTE_RED, 2));
    lv_style_set_width(&red_indicator_style, 10U);
    lv_style_set_line_width(&red_indicator_style, 2U);

    lv_style_init(&red_ticks_style);
    lv_style_set_line_color(&red_ticks_style, lv_palette_darken(LV_PALETTE_RED, 2));
    lv_style_set_width(&red_ticks_style, 5U);
    lv_style_set_line_width(&red_ticks_style, 2U);

    lv_style_init(&orange_indicator_style);
    lv_style_set_line_color(&orange_indicator_style, lv_palette_darken(LV_PALETTE_ORANGE, 2));
    lv_style_set_width(&orange_indicator_style, 10U);
    lv_style_set_line_width(&orange_indicator_style, 2U);

    lv_style_init(&orange_ticks_style);
    lv_style_set_line_color(&orange_ticks_style, lv_palette_darken(LV_PALETTE_ORANGE, 2));
    lv_style_set_width(&orange_ticks_style, 5U);
    lv_style_set_line_width(&orange_ticks_style, 2U);

    lv_style_init(&green_indicator_style);
    lv_style_set_line_color(&green_indicator_style, lv_palette_darken(LV_PALETTE_GREEN, 2));
    lv_style_set_width(&green_indicator_style, 10U);
    lv_style_set_line_width(&green_indicator_style, 2U);

    lv_style_init(&green_ticks_style);
    lv_style_set_line_color(&green_ticks_style, lv_palette_darken(LV_PALETTE_GREEN, 2));
    lv_style_set_width(&green_ticks_style, 5U);
    lv_style_set_line_width(&green_ticks_style, 2U);

    lv_scale_section_t* section = lv_scale_add_section(ui_objects.v5_scale);
    lv_scale_set_section_range(ui_objects.v5_scale, section, 460, 466);
    lv_scale_set_section_style_indicator(ui_objects.v5_scale, section, &red_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v5_scale, section, &red_ticks_style);

    section = lv_scale_add_section(ui_objects.v5_scale);
    lv_scale_set_section_range(ui_objects.v5_scale, section, 466, 484);
    lv_scale_set_section_style_indicator(ui_objects.v5_scale, section, &orange_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v5_scale, section, &orange_ticks_style);

    section = lv_scale_add_section(ui_objects.v5_scale);
    lv_scale_set_section_range(ui_objects.v5_scale, section, 484, 515);
    lv_scale_set_section_style_indicator(ui_objects.v5_scale, section, &green_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v5_scale, section, &green_ticks_style);

    section = lv_scale_add_section(ui_objects.v5_scale);
    lv_scale_set_section_range(ui_objects.v5_scale, section, 515, 532);
    lv_scale_set_section_style_indicator(ui_objects.v5_scale, section, &orange_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v5_scale, section, &orange_ticks_style);

    section = lv_scale_add_section(ui_objects.v5_scale);
    lv_scale_set_section_range(ui_objects.v5_scale, section, 532, 540);
    lv_scale_set_section_style_indicator(ui_objects.v5_scale, section, &red_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v5_scale, section, &red_ticks_style);

    ui_objects.v5_needle = lv_line_create(lv_screen_active());
    lv_obj_set_style_line_color(ui_objects.v5_needle, lv_palette_darken(LV_PALETTE_PURPLE, 1), LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_objects.v5_needle, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ui_objects.v5_needle, true, LV_PART_MAIN);

    ui_objects.text_v5_label = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_v5_label, "5V");
    lv_obj_set_style_text_color(ui_objects.text_v5_label, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_font(ui_objects.text_v5_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(ui_objects.text_v5_label, x_loc + 25, y_loc - 30);

    ui_objects.text_v5_current = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_v5_current, "0.00 A");
    lv_obj_set_style_text_color(ui_objects.text_v5_current, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_font(ui_objects.text_v5_current, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(ui_objects.text_v5_current, x_loc + 15, y_loc + scale_h + 10);

    ui_objects.v5_needle_width = needle_w;
    V5_set_needle(ui_objects.v5_needle, 5.00, scale_h, scale_w, x_loc, y_loc, 4.65, 5.35, needle_w);
}

static void ps_create_voltage_scale_v12(int32_t x_loc, int32_t y_loc, int32_t scale_w, int32_t scale_h, int32_t needle_w)
{
    ui_objects.v12_scale = lv_scale_create(lv_screen_active());
    lv_obj_set_size(ui_objects.v12_scale, scale_w, scale_h);
    lv_scale_set_label_show(ui_objects.v12_scale, true);
    lv_scale_set_mode(ui_objects.v12_scale, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_obj_set_x(ui_objects.v12_scale, x_loc);
    lv_obj_set_y(ui_objects.v12_scale, y_loc);

    lv_scale_set_total_tick_count(ui_objects.v12_scale, 41);
    lv_scale_set_major_tick_every(ui_objects.v12_scale, 10);

    lv_obj_set_style_length(ui_objects.v12_scale, 10, LV_PART_INDICATOR);
    lv_obj_set_style_length(ui_objects.v12_scale, 5, LV_PART_ITEMS);

    lv_scale_set_range(ui_objects.v12_scale, 1120, 1280);

    static const char* v12_labels[] = { "11.20", "11.60", "12.00", "12.40", "12.80", NULL };
    lv_scale_set_text_src(ui_objects.v12_scale, v12_labels);

    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);
    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&indicator_style, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_line_color(&indicator_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_width(&indicator_style, 10U);
    lv_style_set_line_width(&indicator_style, 2U);
    lv_obj_add_style(ui_objects.v12_scale, &indicator_style, LV_PART_INDICATOR);

    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    lv_style_set_line_color(&minor_ticks_style, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_width(&minor_ticks_style, 5U);
    lv_style_set_line_width(&minor_ticks_style, 2U);
    lv_obj_add_style(ui_objects.v12_scale, &minor_ticks_style, LV_PART_ITEMS);

    static lv_style_t red_indicator_style, red_ticks_style;
    static lv_style_t orange_indicator_style, orange_ticks_style;
    static lv_style_t green_indicator_style, green_ticks_style;

    lv_style_init(&red_indicator_style);
    lv_style_set_line_color(&red_indicator_style, lv_palette_darken(LV_PALETTE_RED, 2));
    lv_style_set_width(&red_indicator_style, 10U);
    lv_style_set_line_width(&red_indicator_style, 2U);

    lv_style_init(&red_ticks_style);
    lv_style_set_line_color(&red_ticks_style, lv_palette_darken(LV_PALETTE_RED, 2));
    lv_style_set_width(&red_ticks_style, 5U);
    lv_style_set_line_width(&red_ticks_style, 2U);

    lv_style_init(&orange_indicator_style);
    lv_style_set_line_color(&orange_indicator_style, lv_palette_darken(LV_PALETTE_ORANGE, 2));
    lv_style_set_width(&orange_indicator_style, 10U);
    lv_style_set_line_width(&orange_indicator_style, 2U);

    lv_style_init(&orange_ticks_style);
    lv_style_set_line_color(&orange_ticks_style, lv_palette_darken(LV_PALETTE_ORANGE, 2));
    lv_style_set_width(&orange_ticks_style, 5U);
    lv_style_set_line_width(&orange_ticks_style, 2U);

    lv_style_init(&green_indicator_style);
    lv_style_set_line_color(&green_indicator_style, lv_palette_darken(LV_PALETTE_GREEN, 2));
    lv_style_set_width(&green_indicator_style, 10U);
    lv_style_set_line_width(&green_indicator_style, 2U);

    lv_style_init(&green_ticks_style);
    lv_style_set_line_color(&green_ticks_style, lv_palette_darken(LV_PALETTE_GREEN, 2));
    lv_style_set_width(&green_ticks_style, 5U);
    lv_style_set_line_width(&green_ticks_style, 2U);

    lv_scale_section_t* section = lv_scale_add_section(ui_objects.v12_scale);
    lv_scale_set_section_range(ui_objects.v12_scale, section, 1120, 1133);
    lv_scale_set_section_style_indicator(ui_objects.v12_scale, section, &red_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v12_scale, section, &red_ticks_style);

    section = lv_scale_add_section(ui_objects.v12_scale);
    lv_scale_set_section_range(ui_objects.v12_scale, section, 1133, 1170);
    lv_scale_set_section_style_indicator(ui_objects.v12_scale, section, &orange_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v12_scale, section, &orange_ticks_style);

    section = lv_scale_add_section(ui_objects.v12_scale);
    lv_scale_set_section_range(ui_objects.v12_scale, section, 1170, 1229);
    lv_scale_set_section_style_indicator(ui_objects.v12_scale, section, &green_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v12_scale, section, &green_ticks_style);

    section = lv_scale_add_section(ui_objects.v12_scale);
    lv_scale_set_section_range(ui_objects.v12_scale, section, 1229, 1264);
    lv_scale_set_section_style_indicator(ui_objects.v12_scale, section, &orange_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v12_scale, section, &orange_ticks_style);

    section = lv_scale_add_section(ui_objects.v12_scale);
    lv_scale_set_section_range(ui_objects.v12_scale, section, 1264, 1280);
    lv_scale_set_section_style_indicator(ui_objects.v12_scale, section, &red_indicator_style);
    lv_scale_set_section_style_items(ui_objects.v12_scale, section, &red_ticks_style);

    ui_objects.v12_needle = lv_line_create(lv_screen_active());
    lv_obj_set_style_line_color(ui_objects.v12_needle, lv_palette_darken(LV_PALETTE_PURPLE, 1), LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_objects.v12_needle, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ui_objects.v12_needle, true, LV_PART_MAIN);

    ui_objects.text_v12_label = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_v12_label, "12V");
    lv_obj_set_style_text_color(ui_objects.text_v12_label, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_font(ui_objects.text_v12_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(ui_objects.text_v12_label, x_loc + 20, y_loc - 30);

    ui_objects.text_v12_current = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_v12_current, "0.00 A");
    lv_obj_set_style_text_color(ui_objects.text_v12_current, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_font(ui_objects.text_v12_current, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(ui_objects.text_v12_current, x_loc + 15, y_loc + scale_h + 10);

    ui_objects.v12_needle_width = needle_w;
    V12_set_needle(ui_objects.v12_needle, 12.0, scale_h, scale_w, x_loc, y_loc, 11.2, 12.8, needle_w);
}

static void ps_create_left_metrics(void)
{
    const int32_t left_margin = 0;
    const int32_t left_width = 100;
    const int32_t top_start = 30;
    const int32_t metric_spacing = 20;

    /* Wall Power Value (Very Large) */
    ui_objects.text_wall_power = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(ui_objects.text_wall_power, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_objects.text_wall_power, "500.0W");
    lv_obj_set_style_text_font(ui_objects.text_wall_power, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_objects.text_wall_power, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_width(ui_objects.text_wall_power, left_width + 20);
    lv_obj_set_style_text_align(ui_objects.text_wall_power, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(ui_objects.text_wall_power, 500, 0);
    lv_obj_set_pos(ui_objects.text_wall_power, left_margin, top_start);

    /* Wall Power Label */
    lv_obj_t* text_wall_power_label = lv_label_create(lv_screen_active());
    lv_label_set_text(text_wall_power_label, "Wall Power");
    lv_obj_set_style_text_color(text_wall_power_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_width(text_wall_power_label, left_width);
    lv_obj_set_style_text_align(text_wall_power_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(text_wall_power_label, 200, 0);
    lv_obj_set_pos(text_wall_power_label, left_margin + 15, top_start + 46);

    /* System Power Value */
    ui_objects.text_system_power = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(ui_objects.text_system_power, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_objects.text_system_power, "450.0W");
    lv_obj_set_style_text_color(ui_objects.text_system_power, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_width(ui_objects.text_system_power, left_width);
    lv_obj_set_style_text_align(ui_objects.text_system_power, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(ui_objects.text_system_power, 490, 0);
    lv_obj_set_pos(ui_objects.text_system_power, left_margin - 45, top_start + metric_spacing + 55);

    /* System Power Label */
    lv_obj_t* text_system_power_label = lv_label_create(lv_screen_active());
    lv_label_set_text(text_system_power_label, "System Power");
    lv_obj_set_style_text_color(text_system_power_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_width(text_system_power_label, left_width + 5);
    lv_obj_set_style_text_align(text_system_power_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(text_system_power_label, 200, 0);
    lv_obj_set_pos(text_system_power_label, left_margin + 30, top_start + metric_spacing + 85);

    /* Efficiency Value */
    ui_objects.text_efficiency = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(ui_objects.text_efficiency, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_objects.text_efficiency, "00.0%");
    lv_obj_set_style_text_font(ui_objects.text_efficiency, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_objects.text_efficiency, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_width(ui_objects.text_efficiency, left_width);
    lv_obj_set_style_text_align(ui_objects.text_efficiency, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(ui_objects.text_efficiency, 550, 0);
    lv_obj_set_pos(ui_objects.text_efficiency, left_margin - 10, top_start + (metric_spacing * 2) + 100);

    /* Efficiency Label */
    lv_obj_t* text_efficiency_label = lv_label_create(lv_screen_active());
    lv_label_set_text(text_efficiency_label, "Efficiency");
    lv_obj_set_style_text_color(text_efficiency_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_width(text_efficiency_label, left_width);
    lv_obj_set_style_text_align(text_efficiency_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(text_efficiency_label, 200, 0);
    lv_obj_set_pos(text_efficiency_label, left_margin + 12, top_start + (metric_spacing * 2) + 145);

    /* Temperature Value (Bottom Left) */
    ui_objects.text_temperature = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_temperature, "25.0 °C");
    lv_obj_set_style_text_font(ui_objects.text_temperature, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui_objects.text_temperature, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_width(ui_objects.text_temperature, left_width);
    lv_obj_set_style_text_align(ui_objects.text_temperature, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(ui_objects.text_temperature, 400, 0);
    lv_obj_set_pos(ui_objects.text_temperature, left_margin - 15, VERT_RES - 40);

    /* Last Fault Label */
    ui_objects.text_last_fault = lv_label_create(lv_screen_active());
    lv_label_set_text(ui_objects.text_last_fault, "Last: None");
    lv_obj_set_style_text_font(ui_objects.text_last_fault, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui_objects.text_last_fault, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_width(ui_objects.text_last_fault, left_width + 30);
    lv_obj_set_style_text_align(ui_objects.text_last_fault, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_scale(ui_objects.text_last_fault, 200, 0);
    lv_obj_set_pos(ui_objects.text_last_fault, left_margin + 150, VERT_RES - 30);
}

static void ps_update_left_metrics(void)
{
    static char buffer[32];

    snprintf(buffer, sizeof(buffer), "%.2f W", ac_power_input);
    lv_label_set_text(ui_objects.text_wall_power, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f W", dc_power_output);
    lv_label_set_text(ui_objects.text_system_power, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f %%", sys_efficiency);
    lv_label_set_text(ui_objects.text_efficiency, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f °C", system_temperature);
    lv_label_set_text(ui_objects.text_temperature, buffer);

    const char* fault = supervisor_last_fault();
    snprintf(buffer, sizeof(buffer), "Last: %s", fault);
    lv_label_set_text(ui_objects.text_last_fault, buffer);
    lv_color_t col = (strcmp(fault, "None") == 0)
        ? lv_palette_lighten(LV_PALETTE_GREY, 2)
        : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_text_color(ui_objects.text_last_fault, col, 0);
}

void ps_set_values(float v12v, float v12a,
    float v5v, float v5a,
    float v33v, float v33a,
    float ac_w, float dc_w,
    float eff, float temp_c)
{
    v12_rail_voltage = v12v;
    v12_rail_current = v12a;
    v5_rail_voltage = v5v;
    v5_rail_current = v5a;
    v33_rail_voltage = v33v;
    v33_rail_current = v33a;
    ac_power_input = ac_w;
    dc_power_output = dc_w;
    sys_efficiency = eff;
    system_temperature = temp_c;

    ps_update_v12_needle();
    ps_update_v5_needle();
    ps_update_v33_needle();
    ps_update_left_metrics();

    ps_toggle_status_circle();
}

void ps_gui(void)
{
    /* Main Screen */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);

    /* Create status circle in top left */
    ps_create_status_circle();

    /* Scale dimensions and spacing */
    const int32_t scale_w = 50;
    const int32_t scale_h = 170;
    const int32_t scale_spacing = 70;  /* Space between scales */
    const int32_t right_offset = 180;  /* Shift all scales to the right */
    const int32_t needle_w = 18;       /* Needle width for all scales */

    /* Store dimensions for needle updates */
    ui_objects.scale_w = scale_w;
    ui_objects.scale_h = scale_h;
    ui_objects.scale_spacing = scale_spacing;
    ui_objects.right_offset = right_offset;

    /* Base position - shifted right */
    int32_t x_base = HORI_RES / 2 - scale_w / 2 + right_offset;
    int32_t y_base = VERT_RES / 2 - scale_h / 2;

    /* V12 on the left */
    int32_t v12_x = x_base - (scale_spacing * 2);
    ps_create_voltage_scale_v12(v12_x, y_base, scale_w, scale_h, needle_w);

    /* V5 in the center */
    int32_t v5_x = x_base - scale_spacing;
    ps_create_voltage_scale_v5(v5_x, y_base, scale_w, scale_h, needle_w);

    /* V3.3 on the right */
    int32_t v33_x = x_base;
    ps_create_voltage_scale_v33(v33_x, y_base, scale_w, scale_h, needle_w);

    /* Create left side metrics display */
    ps_create_left_metrics();

    /* Push the example/default values into the GUI so the simulator
     * has something to render even before ps_set_values() is called. */
    ps_set_values(v12_rail_voltage, v12_rail_current,
        v5_rail_voltage, v5_rail_current,
        v33_rail_voltage, v33_rail_current,
        ac_power_input, dc_power_output,
        sys_efficiency, system_temperature);
}
