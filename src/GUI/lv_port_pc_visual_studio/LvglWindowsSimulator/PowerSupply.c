#include "PowerSupply.h"
#include <time.h>
#include <stdio.h>

#define VERT_RES 480
#define HORI_RES 272

/* Global text objects for live updates */
lv_obj_t* live_update_text;
static lv_obj_t* text_value_1;
static lv_obj_t* text_value_2;
static lv_obj_t* text_value_3;
static lv_obj_t* sys_temp_scale;
static lv_obj_t* V33_delta_scale;
static lv_obj_t* screen;

/* Example data that would be updated by your system */
static float sensor_value_1 = 0.0f;
static float sensor_value_2 = 0.0f;
static float sensor_value_3 = 0.0f;

static void ps_create_sys_temp_scale(void)
{

    sys_temp_scale = lv_scale_create(lv_screen_active());
    lv_obj_set_size(sys_temp_scale, 125, 125);
    lv_scale_set_label_show(sys_temp_scale, true);
    lv_scale_set_mode(sys_temp_scale, LV_SCALE_MODE_ROUND_OUTER);
    lv_obj_center(sys_temp_scale);

    lv_scale_set_total_tick_count(sys_temp_scale, 21);
    lv_scale_set_major_tick_every(sys_temp_scale, 5);

    lv_obj_set_style_length(sys_temp_scale, 5, LV_PART_ITEMS);
    lv_obj_set_style_length(sys_temp_scale, 10, LV_PART_INDICATOR);
    lv_scale_set_range(sys_temp_scale, 0, 100);

    static const char * custom_labels[] = {"0 °C", "25 °C", "50 °C", "75 °C", "100 °C", NULL};
    lv_scale_set_text_src(sys_temp_scale, custom_labels);

    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);

    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&indicator_style, lv_palette_darken(LV_PALETTE_BLUE, 3));

    lv_style_set_line_color(&indicator_style, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_width(&indicator_style, 10U);      /*Tick length*/
    lv_style_set_line_width(&indicator_style, 2U);  /*Tick width*/
    lv_obj_add_style(sys_temp_scale, &indicator_style, LV_PART_INDICATOR);

    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    lv_style_set_line_color(&minor_ticks_style, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_width(&minor_ticks_style, 5U);         /*Tick length*/
    lv_style_set_line_width(&minor_ticks_style, 2U);    /*Tick width*/
    lv_obj_add_style(sys_temp_scale, &minor_ticks_style, LV_PART_ITEMS);

    static lv_style_t main_line_style;
    lv_style_init(&main_line_style);
    /* Main line properties */
    lv_style_set_arc_color(&main_line_style, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_arc_width(&main_line_style, 2U); /*Tick width*/
    lv_obj_add_style(sys_temp_scale, &main_line_style, LV_PART_MAIN);

    /* Add a section */
    static lv_style_t section_minor_tick_style;
    static lv_style_t section_label_style;
    static lv_style_t section_main_line_style;

    lv_style_init(&section_label_style);
    lv_style_init(&section_minor_tick_style);
    lv_style_init(&section_main_line_style);

    /* Label style properties */
    lv_style_set_text_font(&section_label_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&section_label_style, lv_palette_darken(LV_PALETTE_RED, 3));

    lv_style_set_line_color(&section_label_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_line_width(&section_label_style, 5U); /*Tick width*/

    lv_style_set_line_color(&section_minor_tick_style, lv_palette_lighten(LV_PALETTE_RED, 2));
    lv_style_set_line_width(&section_minor_tick_style, 4U); /*Tick width*/

    /* Main line properties */
    lv_style_set_arc_color(&section_main_line_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_arc_width(&section_main_line_style, 4U); /*Tick width*/

    /* Configure section styles */
    lv_scale_section_t * section = lv_scale_add_section(sys_temp_scale);
    lv_scale_set_section_range(sys_temp_scale, section, 75, 100);
    lv_scale_set_section_style_indicator(sys_temp_scale, section, &section_label_style);
    lv_scale_set_section_style_items(sys_temp_scale, section, &section_minor_tick_style);
    lv_scale_set_section_style_main(sys_temp_scale, section, &section_main_line_style);
}

static void ps_create_V33_scale(void)
{
    V33_delta_scale = lv_scale_create(lv_screen_active());
    lv_obj_set_size(V33_delta_scale, 60, 200);
    lv_scale_set_label_show(V33_delta_scale, true);
    lv_scale_set_mode(V33_delta_scale, LV_SCALE_MODE_VERTICAL_RIGHT);
    lv_obj_set_x(V33_delta_scale, 300);
    lv_obj_set_y(V33_delta_scale, 150);

    lv_obj_set_style_length(V33_delta_scale, 10, LV_PART_INDICATOR);
    lv_obj_set_style_length(V33_delta_scale, 5, LV_PART_ITEMS);
    lv_scale_set_range(V33_delta_scale, 3.00, 3.46);

    static const char* delta_labels[] = {"3.00", "3.10", "3.20", "3.30", "3.40", "3.50"};
    lv_scale_set_text_src(V33_delta_scale, delta_labels);

    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);

    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&indicator_style, lv_palette_darken(LV_PALETTE_BLUE, 3));

    lv_style_set_line_color(&indicator_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_width(&indicator_style, 10U);      /*Tick length*/
    lv_style_set_line_width(&indicator_style, 2U);  /*Tick width*/
    lv_obj_add_style(V33_delta_scale, &indicator_style, LV_PART_INDICATOR);

    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    lv_style_set_line_color(&minor_ticks_style, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_width(&minor_ticks_style, 5U);         /*Tick length*/
    lv_style_set_line_width(&minor_ticks_style, 2U);    /*Tick width*/
    lv_obj_add_style(V33_delta_scale, &minor_ticks_style, LV_PART_ITEMS);

    static lv_style_t main_line_style;
    lv_style_init(&main_line_style);

    lv_style_set_arc_color(&main_line_style, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_arc_width(&main_line_style, 2U); /*Tick width*/
    lv_obj_add_style(V33_delta_scale, &main_line_style, LV_PART_MAIN);

    static lv_style_t section_minor_tick_style;
    static lv_style_t section_label_style;
    static lv_style_t section_main_line_style;

    lv_style_init(&section_label_style);
    lv_style_init(&section_minor_tick_style);
    lv_style_init(&section_main_line_style);

    lv_style_set_text_font(&section_label_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&section_label_style, lv_palette_darken(LV_PALETTE_RED, 3));

    lv_style_set_line_color(&section_label_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_line_width(&section_label_style, 5U); /*Tick width*/

    lv_style_set_line_color(&section_minor_tick_style, lv_palette_lighten(LV_PALETTE_RED, 2));
    lv_style_set_line_width(&section_minor_tick_style, 4U); /*Tick width*/

    lv_style_set_arc_color(&section_main_line_style, lv_palette_darken(LV_PALETTE_RED, 3));
    lv_style_set_arc_width(&section_main_line_style, 4U); /*Tick width*/

    lv_scale_section_t* section = lv_scale_add_section(V33_delta_scale);
    lv_scale_set_section_range(V33_delta_scale, section, 3.30, 3.46);
    lv_scale_set_section_style_indicator(V33_delta_scale, section, &section_label_style);
    lv_scale_set_section_style_items(V33_delta_scale, section, &section_minor_tick_style);
    lv_scale_set_section_style_main(V33_delta_scale, section, &section_main_line_style);

    lv_obj_set_style_bg_color(V33_delta_scale, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(V33_delta_scale, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_pad_left(V33_delta_scale, 5, LV_PART_INDICATOR);
    lv_obj_set_style_radius(V33_delta_scale, 5, LV_PART_INDICATOR);
    lv_obj_set_style_pad_ver(V33_delta_scale, 5, LV_PART_INDICATOR);
}

static void ps_create_live_text_objects(void)
{
    /* Create three text labels for live updates */
    text_value_1 = lv_label_create(lv_screen_active());
    lv_label_set_text(text_value_1, "Value 1: 0.00");
    lv_obj_set_style_text_color(text_value_1, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_text_font(text_value_1, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(text_value_1, 10, 10);

    text_value_2 = lv_label_create(lv_screen_active());
    lv_label_set_text(text_value_2, "Value 2: 0.00");
    lv_obj_set_style_text_color(text_value_2, lv_palette_darken(LV_PALETTE_GREEN, 3), 0);
    lv_obj_set_style_text_font(text_value_2, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(text_value_2, 10, 40);

    text_value_3 = lv_label_create(lv_screen_active());
    lv_label_set_text(text_value_3, "Value 3: 0.00");
    lv_obj_set_style_text_color(text_value_3, lv_palette_darken(LV_PALETTE_RED, 3), 0);
    lv_obj_set_style_text_font(text_value_3, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(text_value_3, 10, 70);
}

void ps_update_live_text_values(void)
{
    static char buffer[64];

    /* Update text object 1 */
    snprintf(buffer, sizeof(buffer), "Value 1: %.2f", sensor_value_1);
    lv_label_set_text(text_value_1, buffer);

    /* Update text object 2 */
    snprintf(buffer, sizeof(buffer), "Value 2: %.2f", sensor_value_2);
    lv_label_set_text(text_value_2, buffer);

    /* Update text object 3 */
    snprintf(buffer, sizeof(buffer), "Value 3: %.2f", sensor_value_3);
    lv_label_set_text(text_value_3, buffer);
}

/* Timer callback function for live updates */
static void ps_update_timer_callback(lv_timer_t* timer)
{
    (void)timer; /* Unused parameter */
    
    /* Simulate sensor data updates (replace with actual sensor readings) */
    sensor_value_1 += 0.1f;
    sensor_value_2 += 0.2f;
    sensor_value_3 += 0.15f;

    ps_update_live_text_values();
}

void ps_gui(void)
{
    /* Main Screen */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);

    /* Create sys_temp_scale display */
    //ps_create_sys_temp_scale();

    /* Create V33_delta_scale display */
    //ps_create_V33_scale();

    /* Create the three live-updating text objects */
    ps_create_live_text_objects();

    /* Create a timer that calls ps_update_timer_callback every 500ms */
    lv_timer_create(ps_update_timer_callback, 500, NULL);
}
