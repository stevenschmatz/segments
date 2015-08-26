#include "pebble.h"

#define BACKGROUND_COLOR GColorFromHEX(0x1E90FF)
#define COUNT_UP false

static const GPathInfo MINUTE_SEGMENT_PATH_POINTS = {
    .num_points = 3,
    .points = (GPoint[]) {
        {0, 0},
        {-3, -62}, // 58 = radius + fudge; 1 = 80*tan(1.2 degrees); 1.2 degrees per second;
        {3,  -62},
    }
};

static const GPathInfo HOUR_SEGMENT_PATH_POINTS = {
    .num_points = 3,
    .points = (GPoint[]) {
        {0, 0},
        {-3, -56}, // 40 = radius + fudge; 1 = 80*tan(6 degrees); 6 degrees per second;
        {3,  -56},
    }
};

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_minute_display_layer, *s_hour_display_layer;
static GPath *s_minute_segment_path, *s_hour_segment_path;

/**
 * Sets the center time label.
 *
 * @param t
 */
static void set_time_label(struct tm *t) {
    int block_number;
    if (COUNT_UP) {
        block_number = (t->tm_hour * 12) + (t->tm_min / 5);
    } else {
        block_number = 288 - ((t->tm_hour * 12) + (t->tm_min / 5));
    }

    // Create a long-lived buffer
    static char buffer[] = "0";
    snprintf(buffer, 4, "%d", block_number);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, buffer);
}


static void minute_display_update_proc(Layer *layer, GContext* ctx) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    unsigned int total_seconds = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
    unsigned int angle = (total_seconds % 300) * 360.0 / 300.0;

    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, center, 60);
    graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);

    for(; angle < 360; angle += 1) {
        gpath_rotate_to(s_minute_segment_path, (TRIG_MAX_ANGLE / 360.0) * angle);
        gpath_draw_filled(ctx, s_minute_segment_path);
    }

    graphics_fill_circle(ctx, center, 55);

    set_time_label(t);
}

static void hour_display_update_proc(Layer *layer, GContext* ctx) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    unsigned int total_seconds = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
    unsigned int angle = total_seconds / 240.0;

    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, center, 54);
    graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);

    for(; angle < 360; angle += 1) {
        gpath_rotate_to(s_hour_segment_path, (TRIG_MAX_ANGLE / 360.0) * angle);
        gpath_draw_filled(ctx, s_hour_segment_path);
    }

    graphics_fill_circle(ctx, center, 50);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(s_minute_display_layer);
    layer_mark_dirty(s_hour_display_layer);
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_minute_display_layer = layer_create(bounds);
    layer_set_update_proc(s_minute_display_layer, minute_display_update_proc);
    layer_add_child(window_layer, s_minute_display_layer);

    s_minute_segment_path = gpath_create(&MINUTE_SEGMENT_PATH_POINTS);
    gpath_move_to(s_minute_segment_path, grect_center_point(&bounds));

    s_hour_display_layer = layer_create(bounds);
    layer_set_update_proc(s_hour_display_layer, hour_display_update_proc);
    layer_add_child(window_layer, s_hour_display_layer);

    s_hour_segment_path = gpath_create(&HOUR_SEGMENT_PATH_POINTS);
    gpath_move_to(s_hour_segment_path, grect_center_point(&bounds));

        // Create time TextLayer
    s_time_layer = text_layer_create(GRect(0, 55, 144, 50));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_text(s_time_layer, "0");

    // Improve the layout to be more like a watchface
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

    // Add it as a child layer to the Window's root layer
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
    gpath_destroy(s_minute_segment_path);
    gpath_destroy(s_hour_segment_path);

    layer_destroy(s_minute_display_layer);
    layer_destroy(s_hour_display_layer);
}

static void init() {
    s_main_window = window_create();
    window_set_background_color(s_main_window, BACKGROUND_COLOR);

    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    window_stack_push(s_main_window, true);

    tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}

static void deinit() {
    window_destroy(s_main_window);

    tick_timer_service_unsubscribe();
}

int main() {
    init();
    app_event_loop();
    deinit();
}
