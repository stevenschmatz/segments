#include "pebble.h"

#define BACKGROUND_COLOR GColorFromRGB(0, 179, 219)

static const GPathInfo MINUTE_SEGMENT_PATH_POINTS = {
    3,
    (GPoint[]) {
        {0, 0},
        {-6, -58}, // 80 = radius + fudge; 8 = 80*tan(6 degrees); 6 degrees per minute;
        {6,  -58},
    }
};

static const GPathInfo HOUR_SEGMENT_PATH_POINTS = {
    3,
    (GPoint[]) {
        {0, 0},
        {-6, -58}, // 80 = radius + fudge; 8 = 80*tan(6 degrees); 6 degrees per minute;
        {6,  -58},
    }
};

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_minute_display_layer, *s_hour_display_layer;
static GPath *s_minute_segment_path, *s_hour_segment_path;

static void minute_display_update_proc(Layer *layer, GContext* ctx) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    unsigned int total_seconds = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
    unsigned int angle = (total_seconds % 300) * 360 / 300;

    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, center, 55);
    graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);

    for(; angle < 355; angle += 6) {
        gpath_rotate_to(s_minute_segment_path, (TRIG_MAX_ANGLE / 360) * angle);
        gpath_draw_filled(ctx, s_minute_segment_path);
    }

    graphics_fill_circle(ctx, center, 50);

    // Create a long-lived buffer
    static char buffer[] = "0";
    int block_number = (t->tm_hour * 12) + (t->tm_min / 5);
    snprintf(buffer, 4, "%d", block_number);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, buffer);
}

static void hour_display_update_proc(Layer *layer, GContext* ctx) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    unsigned int total_seconds = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
    unsigned int angle = total_seconds / 240;

    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, center, 49);
    graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);

    for(; angle < 355; angle += 6) {
        gpath_rotate_to(s_hour_segment_path, (TRIG_MAX_ANGLE / 360) * angle);
        gpath_draw_filled(ctx, s_hour_segment_path);
    }

    graphics_fill_circle(ctx, center, 45);
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
