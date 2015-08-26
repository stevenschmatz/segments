#include "pebble.h"

#define BACKGROUND_COLOR GColorPictonBlue
#define CIRCLE_COLOR GColorWhite
#define TEXT_COLOR GColorWhite

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define MINUTES_PER_HOUR 60

#define MIN_PER_SEGMENT 6
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
 */
static void set_time_label(struct tm *t) {
    int block_number;
    if (COUNT_UP) {
        block_number = ((t->tm_hour * (MINUTES_PER_HOUR / MIN_PER_SEGMENT)) + (t->tm_min / MIN_PER_SEGMENT));
    } else {
        block_number =
            (SECONDS_PER_DAY / (SECONDS_PER_MINUTE * MIN_PER_SEGMENT)) -
            ((t->tm_hour * (MINUTES_PER_HOUR / MIN_PER_SEGMENT)) + (t->tm_min / MIN_PER_SEGMENT));
    }

    // Create a long-lived buffer
    static char buffer[] = "0";
    snprintf(buffer, 4, "%d", block_number);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, buffer);
}

/**
 * Draws an incomplete circle clockwise to the given angle.
 */
static void draw_progress_circle(Layer *layer, GContext *ctx, int radius, int border_width, unsigned int angle, bool inner_path) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, CIRCLE_COLOR);
    graphics_fill_circle(ctx, center, radius + border_width);
    graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);

    for(; angle < 360; angle += 1) {
        GPath *circle_path = (inner_path) ?  s_hour_segment_path : s_minute_segment_path;
        gpath_rotate_to(circle_path, (TRIG_MAX_ANGLE / 360.0) * angle);
        gpath_draw_filled(ctx, circle_path);
    }

    graphics_fill_circle(ctx, center, radius);
}

/**
 * Screen update procedure, called every second.
 * Updates the segment number, as well as the circles.
 */
static void render_update_proc(Layer *layer, GContext* ctx) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    unsigned int total_seconds = t->tm_hour * SECONDS_PER_HOUR +
                                 t->tm_min * SECONDS_PER_MINUTE +
                                 t->tm_sec;

    unsigned int segment_angle = (total_seconds % (MIN_PER_SEGMENT * SECONDS_PER_MINUTE)) * (360.0 / (MIN_PER_SEGMENT * SECONDS_PER_MINUTE));
//     unsigned int total_angle = total_seconds / 240.0;

    draw_progress_circle(layer, ctx, 55, 3, segment_angle, false);
    draw_progress_circle(layer, ctx, 50, 3, 50, true);

    set_time_label(t);
}

/**
 * Re-renders the layers every second.
 */
static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(s_minute_display_layer);
    layer_mark_dirty(s_hour_display_layer);
}

/**
 * Creates the minute display layer and path, hour display layer and path, and text layer.
 */
static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_minute_display_layer = layer_create(bounds);
    layer_set_update_proc(s_minute_display_layer, render_update_proc);
    layer_add_child(window_layer, s_minute_display_layer);

    s_minute_segment_path = gpath_create(&MINUTE_SEGMENT_PATH_POINTS);
    gpath_move_to(s_minute_segment_path, grect_center_point(&bounds));

    s_hour_display_layer = layer_create(bounds);
    layer_add_child(window_layer, s_hour_display_layer);

    s_hour_segment_path = gpath_create(&HOUR_SEGMENT_PATH_POINTS);
    gpath_move_to(s_hour_segment_path, grect_center_point(&bounds));

    // Create time TextLayer
    s_time_layer = text_layer_create(GRect(0, 55, 144, 50));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, TEXT_COLOR);
    text_layer_set_text(s_time_layer, "0");

    // Improve the layout to be more like a watchface
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

    // Add it as a child layer to the Window's root layer
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
}

/**
 * Destroys paths drawn on the window.
 */
static void main_window_unload(Window *window) {
    gpath_destroy(s_minute_segment_path);
    gpath_destroy(s_hour_segment_path);
    layer_destroy(s_minute_display_layer);
    layer_destroy(s_hour_display_layer);
}

/**
 * Creates the window.
 */
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

/**
 * Destroys the main window.
 */
static void deinit() {
    window_destroy(s_main_window);
    tick_timer_service_unsubscribe();
}

int main() {
    init();
    app_event_loop();
    deinit();
}
