
/*
 * EQController.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#include "xwidgets.h"


#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 *    helpers
****************************************************************/

static void null_call(void *w_, void *user_data) {
    
}

void adjust_eq_font_size(cairo_t* cr, double font_size, int available_width, const char* label) {
    cairo_text_extents_t extents;
    cairo_set_font_size(cr, font_size);
    cairo_text_extents(cr, label, &extents);
    
    if (extents.width > available_width - 4) {
        double scale = (double)(available_width - 4) / extents.width;
        font_size *= scale;
        font_size = std::max<int>(font_size, 6.0);
        cairo_set_font_size(cr, font_size);
    }
}


/****************************************************************
 *    knob helpers
****************************************************************/

static void show_label(Widget_t *w, int width, int height) {
    //use_text_color_scheme(w, get_color_state(w));
    cairo_set_source_rgba(w->crb, 0.61, 0.649, 0.583, 1.0);
    cairo_text_extents_t extents;
    /** show label below the knob**/
    adjust_eq_font_size (w->crb, (w->app->normal_font * w->app->hdpi)/w->scale.ascale, width, w->label);
    cairo_text_extents(w->crb,w->label , &extents);
    cairo_move_to (w->crb, (width*0.5)-(extents.width/2), height-(extents.height/4));
    cairo_text_path (w->crb, w->label);
    cairo_fill (w->crb);
    cairo_new_path (w->crb);
}

void knobShadowOutset(cairo_t* const cr, int width, int height, int x, int y) {
    cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x + width, y + height);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, 0.33, 0.33, 0.33, 1);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.45, 0.33 * 0.6, 0.33 * 0.6, 0.33 * 0.6, 0.4);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.65, 0.05 * 2.0, 0.05 * 2.0, 0.05 * 2.0, 0.4);
    cairo_pattern_add_color_stop_rgba 
        (pat, 1, 0.05, 0.05, 0.05, 1);
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_NONE);
    cairo_set_source(cr, pat);
    cairo_fill_preserve (cr);
    cairo_pattern_destroy (pat);
}

void knobShadowInset(cairo_t* const cr, int width, int height, int x, int y) {
    cairo_pattern_t* pat = cairo_pattern_create_linear (x, y, x + width, y + height);
    cairo_pattern_add_color_stop_rgba
        (pat, 1, 0.33, 0.33, 0.33, 1);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.65, 0.33 * 0.6, 0.33 * 0.6, 0.33 * 0.6, 0.4);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.55, 0.05 * 2.0, 0.05 * 2.0, 0.05 * 2.0, 0.4);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, 0.05, 0.05, 0.05, 1);
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_NONE);
    cairo_set_source(cr, pat);
    cairo_fill (cr);
    cairo_pattern_destroy (pat);
}

void setKnobFrame(Widget_t* w, int x, int y, int wi, int h) {
    Colors *c = get_color_scheme(w, NORMAL_);
    cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, c->bg[0]*4.5, c->bg[1]*4.5, c->bg[2]*4.5,1.0);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.2, c->bg[0]*3.0, c->bg[1]*3.0, c->bg[2]*3.0,1.0);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.3, c->bg[0]*2.0, c->bg[1]*2.0, c->bg[2]*2.0,1.0);
    cairo_pattern_add_color_stop_rgba 
        (pat, 0.6, c->bg[0]*0.1, c->bg[1]*0.1, c->bg[2]*0.1,1.0);
    cairo_pattern_add_color_stop_rgba 
        (pat, 1, c->bg[0]*0.1, c->bg[1]*0.1, c->bg[2]*0.1,1.0);
    cairo_set_source(w->crb, pat);
    cairo_pattern_destroy (pat);
}

/****************************************************************
 *    knobs
****************************************************************/

static void draw_eq_knob(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    const int width = metrics.width-2;
    const int height = metrics.height - ((w->app->small_font + 9 + w->app->normal_font) * w->app->hdpi);
    if (!metrics.visible) return;

    const int grow = (width > height+9) ? height+9:width;
    const int knob_x = grow-1;
    const int knob_y = grow-1;

    const int knobx = (width - knob_x) * 0.5;
    const int knobx1 = width* 0.5;

    const int knoby = (height+9 - knob_y) * 0.5;
    const int knoby1 = (height+9) * 0.5;

    /** get geometric values for the knob **/
    const double scale_zero = 20 * (M_PI/180); // defines "dead zone"
    const double state = adj_get_state(w->adj);
    const double angle = scale_zero + state * 2 * (M_PI - scale_zero);

    const double pointer_off =knob_x/3.5;
    const double radius = min(knob_x-pointer_off, knob_y-pointer_off) / 2;
    const double lengh_x = (knobx+radius+pointer_off/2) - radius * 0.6 * sin(angle);
    const double lengh_y = (knoby+radius+pointer_off/2) + radius * 0.6 * cos(angle);
    const double radius_x = (knobx+radius+pointer_off/2) - radius*0.85 * sin(angle);
    const double radius_y = (knoby+radius+pointer_off/2) + radius*0.85 * cos(angle);

    /** draw the knob **/
    cairo_push_group (w->crb);
    cairo_text_extents_t extents;

    float body = knob_x/2.4;
    cairo_arc(w->crb,knobx1, knoby1, body, 0, 2 * M_PI );

    cairo_pattern_t *pat = cairo_pattern_create_linear(
        knobx1, knoby1 - body, knobx1, knoby1 + body
    );
    cairo_pattern_add_color_stop_rgb(pat, 0.00, 0.33, 0.33, 0.33);
    cairo_pattern_add_color_stop_rgb(pat, 0.1, 0.20, 0.20, 0.20);
    cairo_pattern_add_color_stop_rgb(pat, 0.25, 0.09, 0.09, 0.09);
    cairo_pattern_add_color_stop_rgb(pat, 0.65, 0.063, 0.063, 0.063);
    cairo_pattern_add_color_stop_rgb(pat, 1.00, 0.033, 0.033, 0.033);
    cairo_set_source(w->crb, pat);
    cairo_fill_preserve(w->crb);
    cairo_pattern_destroy(pat);

    cairo_set_source_rgba(w->crb, 0.16, 0.16, 0.18, 1);
    //cairo_fill_preserve (w->crb);
    //setKnobFrame(w,0, 0, width, height);
    cairo_stroke (w->crb);
    cairo_new_path (w->crb);

    cairo_arc(w->crb,knobx1, knoby1, knob_x/3.1, 0, 2 * M_PI );
    //cairo_set_source_rgba(w->crb, 0.12, 0.12, 0.14, 1);
    use_bg_color_scheme(w, NORMAL_);
    cairo_fill_preserve (w->crb);
    setKnobFrame(w,0, 0, width, height);
    cairo_set_line_width(w->crb,2);
    cairo_stroke (w->crb);
    cairo_new_path (w->crb);

    /** create a rotating pointer on the kob**/
    cairo_set_line_cap(w->crb, CAIRO_LINE_CAP_ROUND); 
    cairo_set_line_join(w->crb, CAIRO_LINE_JOIN_BEVEL);
    cairo_move_to(w->crb, radius_x, radius_y);
    cairo_line_to(w->crb,lengh_x,lengh_y);
    cairo_set_line_width(w->crb,knobx1/10);
    cairo_set_source_rgba(w->crb, 0.893, 0.893, 0.893, 1);
    cairo_stroke_preserve(w->crb);
    cairo_new_path (w->crb);

    use_text_color_scheme(w, get_color_state(w));

    /** show value below the kob**/
    char s[64];
    const char* format[] = {"%.1f %s", "%.2f %s", "%.3f %s"};
    float value = adj_get_value(w->adj);
    if (fabs(value)<10.0) {
        snprintf(s, 63, format[2-1], value, w->input_label);
    } else if (fabs(value)<100.0) {
        snprintf(s, 63, format[1-1], value, w->input_label);
    } else {
        snprintf(s, 63,"%d%s",  (int) value, w->input_label);
    }
    adjust_eq_font_size (w->crb, (w->app->small_font/w->scale.ascale) * w->app->hdpi, width, s);
    cairo_text_extents(w->crb, s, &extents);
    cairo_move_to (w->crb, knobx1-extents.width/2, height + 2 + (w->app->small_font * w->app->hdpi)+extents.height/2);
    cairo_text_path (w->crb, s);
    cairo_fill (w->crb);
    cairo_new_path (w->crb);

    show_label(w, width, height + ((w->app->small_font + 9) + w->app->normal_font) * w->app->hdpi);

    cairo_pop_group_to_source (w->crb);
    cairo_paint (w->crb);
}

void draw_my_knob(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    int width = metrics.width-2;
    int height = metrics.height - (w->app->small_font + 9 + w->app->normal_font) * w->app->hdpi;
    if (!metrics.visible) return;

    const double scale_zero = 20 * (M_PI/180); // defines "dead zone" for knobs
    int arc_offset = 2;
    int knob_x = 0;
    int knob_y = 0;

    int grow = (width > height) ? height:width;
    knob_x = grow-1;
    knob_y = grow-1;
    /** get values for the knob **/
    const int iw = width * 0.1;

    const int knobx1 = width* 0.5;

    const int knoby1 = height * 0.5;

    const double knobstate = adj_get_state(w->adj_y);
    const double angle = scale_zero + knobstate * 2 * (M_PI - scale_zero);

    const double pointer_off =knob_x/6;
    const double radius = min(knob_x-pointer_off, knob_y-pointer_off) / 2;

    const double add_angle = 90 * (M_PI / 180.);

    // base
    use_base_color_scheme(w, INSENSITIVE_);
    cairo_set_line_width(w->crb, iw/w->scale.ascale);
    cairo_arc (w->crb, knobx1, knoby1+arc_offset, radius,
          add_angle + scale_zero, add_angle + scale_zero + 320 * (M_PI/180));
    cairo_stroke(w->crb);

    // indicator
    cairo_new_sub_path(w->crb);
    use_fg_color_scheme(w, NORMAL_);
    cairo_arc (w->crb,knobx1, knoby1+arc_offset, radius,
          add_angle + scale_zero, add_angle + angle);
    cairo_stroke(w->crb);

    use_text_color_scheme(w, get_color_state(w));
    cairo_text_extents_t extents;
    /** show value below the kob**/
    char s[64];
    const char* format[] = {"%.1f %s", "%.2f %s", "%.3f %s"};
    float value = adj_get_value(w->adj);
    if (fabs(value)<10.0) {
        snprintf(s, 63, format[2-1], value, w->input_label);
    } else if (fabs(value)<100.0) {
        snprintf(s, 63, format[1-1], value, w->input_label);
    } else {
        snprintf(s, 63,"%d%s",  (int) value, w->input_label);
    }
    adjust_eq_font_size (w->crb, (w->app->small_font/w->scale.ascale) * w->app->hdpi, width, s);
    cairo_text_extents(w->crb, s, &extents);
    cairo_move_to (w->crb, knobx1-extents.width/2, height + (w->app->small_font * w->app->hdpi)+extents.height/2);
    cairo_text_path (w->crb, s);
    cairo_fill (w->crb);
    cairo_new_path (w->crb);

    show_label(w, width, height + ((w->app->small_font + 9) + w->app->normal_font) * w->app->hdpi);
}

Widget_t* add_eq_knob(Widget_t *parent, const char * label, const char* type,
                int x, int y, int width, int height) {

    Widget_t *wid = add_knob(parent, label, x, y, width, height);
    //wid->flags = USE_TRANSPARENCY | FAST_REDRAW;
    wid->flags |= NO_PROPAGATE;
    set_widget_color(wid, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
    wid->func.expose_callback = draw_eq_knob;
    snprintf(wid->input_label, 31, "%s", type);
    return wid;
}

Widget_t* add_my_knob(Widget_t *parent, const char * label, const char* type,
                int x, int y, int width, int height) {

    Widget_t *wid = add_knob(parent, label, x, y, width, height);
    //wid->flags = USE_TRANSPARENCY | FAST_REDRAW;
    wid->flags |= NO_PROPAGATE;
    set_widget_color(wid, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
    wid->func.expose_callback = draw_my_knob;
    snprintf(wid->input_label, 31, "%s", type);
    return wid;
}

/****************************************************************
 *    panel helpers
****************************************************************/

static void setFrameColour(Widget_t* w, cairo_t *cr, int x, int y, int wi, int h) {
    Colors *c = get_color_scheme(w, NORMAL_);
   // Colors *c1 = get_color_scheme(w, PRELIGHT_);
    cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, c->bg[0]*3.0, c->bg[1]*3.0, c->bg[2]*3.0,1.0);
    cairo_pattern_add_color_stop_rgba 
        (pat, 1, c->bg[0]*2.1, c->bg[1]*2.1, c->bg[2]*2.1,1.0);
    cairo_set_source(cr, pat);
    cairo_pattern_destroy (pat);
}

static void roundrec(cairo_t *cr, float x, float y, float width, float height, float r) {
    cairo_arc(cr, x+r, y+r, r, M_PI, 3*M_PI/2);
    cairo_arc(cr, x+width-r, y+r, r, 3*M_PI/2, 0);
    cairo_arc(cr, x+width-r, y+height-r, r, 0, M_PI/2);
    cairo_arc(cr, x+r, y+height-r, r, M_PI/2, M_PI);
    cairo_close_path(cr);
}


/****************************************************************
 *    frame panel
****************************************************************/

static void draw_frame(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int width_t = metrics.width;
    int height_t = metrics.height;

    cairo_set_line_width(w->crb,2);
    cairo_set_source_rgba(w->crb, 0.15,0.15,0.17,1.0);
    roundrec(w->crb, 2, 0, width_t-4, height_t, 5);
    cairo_fill_preserve(w->crb);

    setFrameColour(w, w->crb, 5, 5, width_t-10, height_t-10);
    cairo_stroke(w->crb);
    cairo_new_path (w->crb);

}

Widget_t* add_my_frame(Widget_t *parent, const char * label,
                int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    wid->label = label;
    wid->flags |= NO_PROPAGATE;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_frame;
    return wid;
}


/****************************************************************
 *    overlay panel
****************************************************************/

static void draw_z_frame(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int width_t = metrics.width;
    int height_t = metrics.height;

    cairo_set_line_width(w->crb,2);
    cairo_set_source_rgba(w->crb, 0.15,0.15,0.17,1.0);
    roundrec(w->crb, 2, 20 * w->app->hdpi, width_t-4, height_t- (20 * w->app->hdpi), 5);
    cairo_fill_preserve(w->crb);

    setFrameColour(w, w->crb, 5, 25, width_t-10, height_t-30);
    cairo_stroke(w->crb);
    cairo_new_path (w->crb);

}

Widget_t* add_my_z_frame(Widget_t *parent, const char * label,
                int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    wid->label = label;
    //wid->flags |= NO_PROPAGATE;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_z_frame;
    return wid;
}


/****************************************************************
 *    EQ controller panel
****************************************************************/

static void setPanelColour(Widget_t* w, cairo_t *cr, int x, int y, int wi, int h) {
    Colors *c = get_color_scheme(w, NORMAL_);
   // Colors *c1 = get_color_scheme(w, PRELIGHT_);
    cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, c->bg[0], c->bg[1], c->bg[2],1.0);
    cairo_pattern_add_color_stop_rgba 
        (pat, 1, c->bg[0]*0.1, c->bg[1]*0.1, c->bg[2]*0.1,1.0);
    cairo_set_source(cr, pat);
    cairo_pattern_destroy (pat);
}

static void draw_panel(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int width_t = metrics.width;
    int height_t = metrics.height;
    cairo_set_line_width(w->crb,2);

    cairo_move_to(w->crb, 0, height_t);
    cairo_curve_to(w->crb, 0, height_t*0.2, width_t*0.15, 1, width_t*0.5, 1);
    cairo_curve_to(w->crb, width_t*0.85, 1, width_t, height_t*0.2, width_t, height_t);
    cairo_close_path(w->crb);

    cairo_set_source_rgba(w->crb, 0.13,0.13,0.15,1.0);
    cairo_fill_preserve(w->crb);
    setPanelColour(w, w->crb, 5, 5, width_t-10, height_t-10);
    cairo_stroke(w->crb);
    cairo_new_path (w->crb);
    
}

Widget_t* add_my_panel(Widget_t *parent, const char * label,
                int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    wid->label = label;
    //wid->flags |= NO_PROPAGATE;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_panel;
    return wid;
}


/****************************************************************
 *    power button
****************************************************************/

void draw_power_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int width  = metrics.width-2;
    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on

    float offset = 0.0f;
    if (w->state == 2) offset = 1.0f; // pressed

    const float cx = width * 0.6f + offset;
    const float cy = height * 0.5f + offset;
    const float r  = (width < height ? width : height) * 0.35f;

    Colors *c = get_color_scheme(w, NORMAL_);
    
    float cr = c->fg[0], cg = c->fg[1], cb = c->fg[2];

    float off = 0.35f;

    if (!state) {
        cr *= off;
        cg *= off;
        cb *= off;
    }

    float alpha = 1.0f;
    if (w->state == 1) alpha = 1.2f;
    float start_angle = -M_PI * 0.4;
    float end_angle   =  M_PI * 1.4;

    cairo_arc(w->crb, cx, cy, r, start_angle, end_angle);
    cairo_set_line_width(w->crb, 2.2);
    cairo_set_source_rgba(w->crb, cr * alpha, cg * alpha, cb * alpha, 1.0);
    cairo_stroke(w->crb);

    float line_len = r * 0.9f;
    cairo_move_to(w->crb, cx, cy - line_len);
    cairo_line_to(w->crb, cx, cy - r * 0.1f);
    cairo_set_line_width(w->crb, 2.2);
    cairo_stroke(w->crb);

    if (state) {
        cairo_arc(w->crb, cx, cy, r + 1.5f, 0, 2 * M_PI);
        cairo_set_source_rgba(w->crb, cr, cg, cb, 0.15);
        cairo_set_line_width(w->crb, 3.0);
        cairo_stroke(w->crb);
    }

    cairo_new_path(w->crb);
}

Widget_t *add_my_enable_button(Widget_t *parent, int x, int y, int width, int height, const char *label) {
    Widget_t *fbutton = add_toggle_button(parent, label, x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE;
    fbutton->func.expose_callback = draw_power_button;
    return fbutton;
}

/****************************************************************
 *    quit button
****************************************************************/

void draw_quit_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int width  = metrics.width-2;
    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    double ac = state ? 0.5 : 0.0;

    float offset = 0.0f;
    if (w->state == 2) offset = 1.0f; // pressed

    const float cx = width * 0.6f + offset;
    const float cy = height * 0.5f + offset;
    const float r  = (width < height ? width : height) * 0.35f;

    cairo_set_source_rgba(w->crb, 0.91 , 0.949 - ac, 0.838 - ac, 0.35 + ac);
    if (w->state) cairo_set_source_rgba(w->crb, 0.91, 0.949 - ac, 0.883 - ac, 0.55);
    if (state) cairo_set_source_rgba(w->crb, 0.15, 0.52, 0.55, 0.96);

    float start_angle = -M_PI * 0.4;
    float end_angle   =  M_PI * 1.4;

    cairo_arc(w->crb, cx, cy, r, start_angle, end_angle);
    cairo_set_line_width(w->crb, 2.2);
    cairo_stroke(w->crb);

    float line_len = r * 0.9f;
    cairo_move_to(w->crb, cx, cy - line_len);
    cairo_line_to(w->crb, cx, cy - r * 0.1f);
    cairo_set_line_width(w->crb, 2.2);
    cairo_stroke(w->crb);

    if (state) {
        cairo_arc(w->crb, cx, cy, r , start_angle, end_angle);
        cairo_set_line_width(w->crb, 3.0);
        cairo_stroke(w->crb);
    }

    cairo_new_path(w->crb);
}

Widget_t *add_my_quit_button(Widget_t *parent, int x, int y, int width, int height, const char *label) {
    Widget_t *fbutton = add_button(parent, label, x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE;
    fbutton->func.expose_callback = draw_quit_button;
    return fbutton;
}

/****************************************************************
 *    mode switch (fft/biquad) button
****************************************************************/

void draw_icon_fft(cairo_t *cr, double x, double y, double size, int state) {
    double pad = size * 0.16;
    double x0 = x + pad;
    double x1 = x + size - pad;
    double y_top = y + pad;
    double y_bot = y + size - pad;

    cairo_save(cr);

    cairo_set_line_width(cr, size * 0.035);
    if (state) cairo_set_line_width(cr, size * 0.065);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.35);
    cairo_move_to(cr, x0, (y_top + y_bot) * 0.5 + (y_bot - y_top) * 0.28);
    cairo_line_to(cr, x1, (y_top + y_bot) * 0.5 + (y_bot - y_top) * 0.28);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58); 
    cairo_set_line_width(cr, size * 0.09);
    if (state) cairo_set_line_width(cr, size * 0.15);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    double y_flat = y_top + (y_bot - y_top) * 0.20;
    double y_low  = y_top + (y_bot - y_top) * 0.85;
    double x_break = x0 + (x1 - x0) * 0.62;

    cairo_move_to(cr, x0, y_flat);
    cairo_line_to(cr, x_break, y_flat);
    cairo_line_to(cr, x_break + (x1 - x0) * 0.06, y_low);
    cairo_line_to(cr, x1, y_low);
    cairo_stroke(cr);

    cairo_restore(cr);
}

void draw_icon_biquad(cairo_t *cr, double x, double y, double size, int state) {
    double pad = size * 0.16;
    double x0 = x + pad;
    double x1 = x + size - pad;
    double y_top = y + pad;
    double y_bot = y + size - pad;

    cairo_save(cr);

    cairo_set_line_width(cr, size * 0.035);
    if (state) cairo_set_line_width(cr, size * 0.065);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.35);
    cairo_move_to(cr, x0, (y_top + y_bot) * 0.5 + (y_bot - y_top) * 0.28);
    cairo_line_to(cr, x1, (y_top + y_bot) * 0.5 + (y_bot - y_top) * 0.28);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58); 
    cairo_set_line_width(cr, size * 0.09);
    if (state) cairo_set_line_width(cr, size * 0.15);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    double y_flat  = y_top + (y_bot - y_top) * 0.20;
    double y_low   = y_top + (y_bot - y_top) * 0.85;

    cairo_move_to(cr, x0, y_flat);
    cairo_curve_to(cr, x0 + (x1 - x0) * 0.35, y_flat,
            x0 + (x1 - x0) * 0.55, y_low, x1, y_low);
    cairo_stroke(cr);

    cairo_restore(cr);
}

void draw_icon_svf(cairo_t *cr, double x, double y, double size, int state) {
    double pad = size * 0.16;
    double x0 = x + pad;
    double x1 = x + size - pad;
    double y_top = y + pad;
    double y_bot = y + size - pad;

    cairo_save(cr);
    cairo_set_line_width(cr, size * 0.035);
    if (state) cairo_set_line_width(cr, size * 0.065);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.35);
    cairo_move_to(cr, x0, (y_top + y_bot) * 0.5 + (y_bot - y_top) * 0.28);
    cairo_line_to(cr, x1, (y_top + y_bot) * 0.5 + (y_bot - y_top) * 0.28);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
    cairo_set_line_width(cr, size * 0.09);
    if (state) cairo_set_line_width(cr, size * 0.15);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    double y_base = y_top + (y_bot - y_top) * 0.80;
    double y_peak = y_top + (y_bot - y_top) * 0.08;
    double x_mid  = x0 + (x1 - x0) * 0.5;

    cairo_move_to(cr, x0, y_base);
    cairo_curve_to(cr,
        x0 + (x1 - x0) * 0.16, y_base,
        x0 + (x1 - x0) * 0.30, y_peak,
        x_mid, y_peak);
    cairo_curve_to(cr,
        x0 + (x1 - x0) * 0.70, y_peak,
        x0 + (x1 - x0) * 0.84, y_base,
        x1, y_base);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void draw_mode_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    static int last_state = state;
    if (state == 1) {
        draw_icon_biquad(w->crb, 0.0, 0.0, height, w->state);
        tooltip_set_my_text(w, "Biquad Filter Mode");
    } else if (state == 2) {
        draw_icon_svf(w->crb, 0.0, 0.0, height, w->state);
        tooltip_set_my_text(w, "State Variable Filter Mode");
    } else {
        draw_icon_fft(w->crb, 0.0, 0.0, height, w->state);
        tooltip_set_my_text(w, "FFT convolver Mode");
    }
    if (last_state != state) {
        show_tooltip(w);
        last_state = state;
    }
}


void fbutton_released(void *w_, void* button_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    XButtonEvent *xbutton = (XButtonEvent*)button_;
    if (w->flags & HAS_POINTER && xbutton->button == Button1) {
        float v = adj_get_value(w->adj);
        adj_set_value(w->adj, v>1 ? 0.0f : v + 1.0f);
    }
    expose_widget(w);
}

Widget_t *add_my_mode_button(Widget_t *parent, int x, int y, int width, int height) {
    Widget_t *fbutton = add_hslider(parent, "", x, y, width, height);
    set_adjustment(fbutton->adj, 0.0, 0.0, 0.0, 2.0, 1.0, CL_CONTINUOS);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE | HAS_TOOLTIP;
    fbutton->func.expose_callback = draw_mode_button;
    fbutton->func.button_release_callback = fbutton_released;
    return fbutton;
}

/****************************************************************
 *    hf fade button
****************************************************************/

void draw_hf_fadeout(cairo_t *cr, double x, double y, double size, int state, const int active) {
    double pad = state ? size * 0.1 : size * 0.16;
    double ac = active ? 0.5 : 0.0;
    double offset = 0.0f;
    if (state == 2) offset = 1.0f; // pressed
    double x0 = x + pad + offset;
    double x1 = x + size - pad - offset;
    double y_top = y + pad + offset;
    double y_bot = y + size - pad - offset;
    double w = x1 - x0;
    double h = y_bot - y_top;
    double r = state ? size * 0.11 : size * 0.09;


    cairo_save(cr);

    cairo_set_line_width(cr, size * 0.035 + ac);
    cairo_set_source_rgba(cr, 0.91 , 0.949 - ac, 0.838 - ac, 0.35 + ac);
    if (state) cairo_set_source_rgba(cr, 0.91, 0.949 - ac, 0.883 - ac, 0.55);
    if (active) cairo_set_source_rgba(cr, 0.15, 0.52, 0.55, 0.96);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x1 - r, y_top + r, r, -M_PI_2, 0);
    cairo_arc(cr, x1 - r, y_bot - r, r, 0, M_PI_2);
    cairo_arc(cr, x0 + r, y_bot - r, r, M_PI_2, M_PI);
    cairo_arc(cr, x0 + r, y_top + r, r, M_PI, 3 * M_PI_2);
    cairo_close_path(cr);
    cairo_stroke_preserve(cr);
    if (active) {
        cairo_set_source_rgba(cr, 0.15, 0.52, 0.55, 0.15);
        cairo_fill_preserve(cr);
    }
    cairo_stroke(cr);

    // 0 dB reference line
    cairo_set_line_width(cr, size * 0.04);
    if (state) cairo_set_line_width(cr, size * 0.05);
    cairo_move_to(cr, x0, y_top + h * 0.30);
    cairo_line_to(cr, x1, y_top + h * 0.30);
    cairo_stroke(cr);

    // response curve: flat pass-band, then rolls off and visually 
    cairo_pattern_t *pat = cairo_pattern_create_linear(x0, 0, x1, 0);
    if (!state) {
        cairo_pattern_add_color_stop_rgba(pat, 0.00, 0.55, 0.55, 0.58, 1.0);
        cairo_pattern_add_color_stop_rgba(pat, 0.55, 0.55, 0.55, 0.58, 1.0);
        cairo_pattern_add_color_stop_rgba(pat, 1.00, 0.55, 0.55, 0.58, 0.12);
    } else {
        cairo_pattern_add_color_stop_rgba(pat, 0.00, 0.91, 0.949, 0.883, 1.0);
        cairo_pattern_add_color_stop_rgba(pat, 0.55, 0.91, 0.949, 0.883, 1.0);
        cairo_pattern_add_color_stop_rgba(pat, 1.00, 0.91, 0.949, 0.883, 0.12);
    }
    cairo_set_source(cr, pat);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    cairo_move_to(cr, x0, y_top + h * 0.30);
    cairo_line_to(cr, x0 + w * 0.55, y_top + h * 0.30);
    cairo_curve_to(cr, x0 + w * 0.72, y_top + h * 0.30,
                        x0 + w * 0.80, y_top + h * 0.55,
                        x0 + w * 0.90, y_top + h * 0.80);
    cairo_curve_to(cr, x0 + w * 0.94, y_top + h * 0.92,
                        x0 + w * 0.97, y_top + h * 0.97,
                        x1, y_bot);
    cairo_stroke(cr);
    cairo_pattern_destroy(pat);

    cairo_restore(cr);
}

void draw_fade_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    draw_hf_fadeout(w->crb, 0.0, 0.0, height, w->state, state);
    tooltip_set_my_text(w, "HF Fade out");
}

Widget_t *add_my_fade_button(Widget_t *parent, int x, int y, int width, int height) {
    Widget_t *fbutton = add_toggle_button(parent, "", x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE | HAS_TOOLTIP;
    fbutton->func.expose_callback = draw_fade_button;
    return fbutton;
}

/****************************************************************
 *    bypass button
****************************************************************/

void draw_bypass(cairo_t *cr, double x, double y, double size,
                                int state, const int active) {
    double pad = state ? size * 0.10 : size * 0.16;
    double ac = active ? 0.5 : 0.0;
    double offset = 0.0f;
    if (state == 2) offset = 1.0f; // pressed
    double x0 = x + pad + offset;
    double x1 = x + size - pad - offset;
    double y0 = y + pad + offset;
    double y1 = y + size - pad - offset;
    double w = x1 - x0;
    double h = y1 - y0;
    double r = state ? size * 0.11 : size * 0.09;

    cairo_save(cr);
    // frame
    cairo_set_line_width(cr, size * 0.035);
    cairo_set_source_rgba(cr, 0.91, 0.949 - ac, 0.838 - ac, 0.35 + ac);
    if (state) cairo_set_source_rgba(cr, 0.91, 0.949 - ac, 0.883 - ac, 0.55);
    if (active) cairo_set_source_rgba(cr, 0.15, 0.52, 0.55, 0.96);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x1 - r, y0 + r, r, -M_PI_2, 0);
    cairo_arc(cr, x1 - r, y1 - r, r, 0, M_PI_2);
    cairo_arc(cr, x0 + r, y1 - r, r, M_PI_2, M_PI);
    cairo_arc(cr, x0 + r, y0 + r, r, M_PI, 3 * M_PI_2);
    cairo_close_path(cr);
    cairo_stroke_preserve(cr);
    double gap = 0.0;
    if (active) {
        cairo_set_source_rgba(cr, 0.15, 0.52, 0.55, 0.15);
        cairo_fill_preserve(cr);
        gap = w * 0.12;
    }
    cairo_stroke(cr);
    // Symbol
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    double cy = y0 + h * 0.40;
    cairo_set_line_width(cr, size * 0.04);
    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
    if (state) cairo_set_source_rgb(cr, 0.91, 0.949, 0.883);
    cairo_move_to(cr, x0, cy);
    cairo_line_to(cr, x0 + w * 0.43- gap, cy);
    cairo_move_to(cr, x1 - w * 0.43 + gap, cy);
    cairo_line_to(cr, x1, cy);
    cairo_stroke(cr);
    cy = y0 + h * 0.60;
    cairo_move_to(cr, x0, cy);
    cairo_line_to(cr, x0 + w * 0.43- gap, cy);
    cairo_move_to(cr, x1 - w * 0.43 + gap, cy);
    cairo_line_to(cr, x1, cy);
    cairo_stroke(cr);

    cy = y0 + h * 0.50;
    cairo_set_line_width(cr, size * 0.05);
    cairo_set_source_rgba(cr, 0.55, 0.55, 0.58, 0.5);
    if (active) cairo_set_source_rgb(cr, 0.15, 0.52, 0.55);
    cairo_move_to(cr, x + size * 0.47, cy + size * 0.15);
    cairo_line_to(cr, x + size * 0.53, cy - size * 0.15);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void draw_bypass_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    draw_bypass(w->crb, 0.0, 0.0, height, w->state, state);
    tooltip_set_my_text(w, "Bypass");
}

Widget_t *add_my_bypass_button(Widget_t *parent, int x, int y, int width, int height) {
    Widget_t *fbutton = add_toggle_button(parent, "", x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE | HAS_TOOLTIP;
    fbutton->func.expose_callback = draw_bypass_button;
    return fbutton;
}


/****************************************************************
 *    combobox
****************************************************************/

void draw_combobox_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    int width = metrics.width-2;
    int height = metrics.height-2;
    if (!metrics.visible) return;
    if (!w->state && (int)w->adj_y->value)
        w->state = 3;

    if(w->state==0) {
        cairo_set_line_width(w->crb, 1.0);
         use_frame_color_scheme(w, PRELIGHT_);
    } else if(w->state==1) {
        cairo_set_line_width(w->crb, 1.5);
        use_frame_color_scheme(w, PRELIGHT_);
    } else if(w->state==2) {
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, PRELIGHT_);
    } else if(w->state==3) {
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, PRELIGHT_);
    }
    cairo_stroke(w->crb); 

    float offset = 0.0;
    if(w->state==0) {
        use_fg_color_scheme(w, NORMAL_);
    } else if(w->state==1) {
        use_fg_color_scheme(w, PRELIGHT_);
        offset = 1.0;
    } else if(w->state==2) {
        use_fg_color_scheme(w, SELECTED_);
        offset = 2.0;
    } else if(w->state==3) {
        use_fg_color_scheme(w, ACTIVE_);
        offset = 1.0;
    }
    use_text_color_scheme(w, get_color_state(w));
    int wa = width/1.1;
    int h = height/2.2;
    int wa1 = width/1.55;
    int h1 = height/1.3;
    int wa2 = width/2.8;
   
    cairo_move_to(w->crb, wa+offset, h+offset);
    cairo_line_to(w->crb, wa1+offset, h1+offset);
    cairo_line_to(w->crb, wa2+offset, h+offset);
    cairo_line_to(w->crb, wa+offset, h+offset);
    cairo_fill(w->crb);
   
}


void draw_eq_combobox(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    int width = metrics.width-2;
    int height = metrics.height-2;
    if (!metrics.visible) return;
    int v = (int)adj_get_value(w->adj);
    int vl = v - (int) w->adj->min_value;
   // if (v<0) return;
    Widget_t * menu = w->childlist->childs[1];
    Widget_t* view_port =  menu->childlist->childs[0];
    ComboBox_t *comboboxlist = (ComboBox_t*)view_port->parent_struct;

    cairo_rectangle(w->crb,2.0, 2.0, width, height);

    if(w->state==0) {
        cairo_set_line_width(w->crb, 1.0);
        use_shadow_color_scheme(w, NORMAL_);
        cairo_fill_preserve(w->crb);
        use_frame_color_scheme(w, NORMAL_);
    } else if(w->state==1) {
        use_shadow_color_scheme(w, PRELIGHT_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.5);
        use_frame_color_scheme(w, NORMAL_);
    } else if(w->state==2) {
        use_shadow_color_scheme(w, SELECTED_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, SELECTED_);
    } else if(w->state==3) {
        use_shadow_color_scheme(w, ACTIVE_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, ACTIVE_);
    } else if(w->state==4) {
        use_shadow_color_scheme(w, INSENSITIVE_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, INSENSITIVE_);
    }
    cairo_stroke(w->crb);

    cairo_rectangle(w->crb,4.0, 4.0, width, height);
    cairo_stroke(w->crb);
    cairo_rectangle(w->crb,3.0, 3.0, width, height);
    cairo_stroke(w->crb);
    if (comboboxlist->list_size<1) return;
    if (vl<0) return;

    int width_t, height_t;
    os_get_surface_size(w->image, &width_t, &height_t);
    double x = (double)height/(double)height_t;
    double y = (double)height_t/(double)height;

    int findex = 2 - vl;
    int frame = width_t/3;

    cairo_save(w->crb);
    cairo_scale(w->crb, x,x);
    cairo_set_source_surface (w->crb, w->image, -frame*findex, 0);
    cairo_rectangle(w->crb, 0, 0, frame, height_t);
    cairo_fill(w->crb);
    cairo_scale(w->crb, y,y);
    cairo_restore(w->crb);

}

void draw_eq_menu(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    int width = metrics.width;
    int height = metrics.height;
    if (!metrics.visible) return;
    ComboBox_t *comboboxlist = (ComboBox_t*)w->parent_struct;
    Widget_t *p = comboboxlist->combobox;
    use_base_color_scheme(w, NORMAL_);
    cairo_rectangle(w->crb, 0, 0, width, height);
    cairo_fill (w->crb);

    int i = (int)max(0,adj_get_value(w->adj));
    int a = 0;
    int j = (int)comboboxlist->list_size < comboboxlist->show_items+i+1 ? 
      comboboxlist->list_size : comboboxlist->show_items+i+1;
    for(;i<j;i++) {
        if(i == comboboxlist->prelight_item && i == comboboxlist->active_item)
            use_base_color_scheme(w, ACTIVE_);
        else if(i == comboboxlist->prelight_item)
            use_base_color_scheme(w, PRELIGHT_);
        else if (i == comboboxlist->active_item)
            use_base_color_scheme(w, SELECTED_);
        else
            use_base_color_scheme(w,NORMAL_ );
        cairo_rectangle(w->crb, 0, a*comboboxlist->item_height, width, comboboxlist->item_height);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, PRELIGHT_);
        cairo_stroke(w->crb); 
        //cairo_text_extents_t extents;
        /** show label **/
        
        int width_t, height_t;
        os_get_surface_size(p->image, &width_t, &height_t);
        double x = (double)comboboxlist->item_height/(double)height_t;
        double y = (double)height_t/(double)comboboxlist->item_height;

        int findex = 2 - i;
        int frame = width_t/3;

        cairo_save(w->crb);
        cairo_scale(w->crb, x,x);
        cairo_set_source_surface (w->crb, p->image, -frame*findex, (a*comboboxlist->item_height)* y);
        cairo_rectangle(w->crb, 0, (a*comboboxlist->item_height)* y, frame, height_t);
        cairo_fill(w->crb);
        cairo_scale(w->crb, y,y);
        cairo_restore(w->crb);
        a++;
    }
}

Widget_t* add_type_combobox(Widget_t *p,const char * label,
                                int x, int y, int width, int height) {
    Widget_t* w = add_combobox(p, label, x, y, width, height);
    w->flags |= NO_PROPAGATE;
    Widget_t * menu = w->childlist->childs[1];
    Widget_t* view_port =  menu->childlist->childs[0];
    ComboBox_t *comboboxlist = (ComboBox_t*)view_port->parent_struct;
    comboboxlist->slider->func.expose_callback = null_call;

    widget_get_png(w, LDVAR(filters_png));
    w->func.expose_callback = draw_eq_combobox;
    view_port->func.expose_callback = draw_eq_menu;
    w->childlist->childs[0]->func.expose_callback = draw_combobox_button;
    return w;
}


/****************************************************************
 *    vumeter
****************************************************************/

float log_meter(float db) {
    if (db < -70.0f) db = -70.0f;
    if (db > 6.0f)   db = 6.0f;
    float norm = (db + 70.0f) / 76.0f;
    return powf(norm, 1.5f);
}

void draw_vmeter_scale(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int rect_width = metrics.width;
    int rect_height = metrics.height;
    double x0      = 0;
    double y0      = 0;

    int  db_points[] = { -60, -48, -36, -24, -12, -6, 0, 3 };
    char  buf[32];

    cairo_set_font_size (w->crb, (float)rect_width * 0.58);
    cairo_set_source_rgb(w->crb, 0.6, 0.6, 0.6);

    for (unsigned int i = 0; i < sizeof (db_points)/sizeof (db_points[0]); ++i)
    {
        float fraction = log_meter((double)db_points[i]);
        cairo_move_to (w->crb, 0,y0+rect_height - (rect_height * fraction));
        cairo_line_to (w->crb, x0+rect_width-3 ,y0+rect_height -  (rect_height * fraction));
        if (i<6)
        {
            snprintf (buf, sizeof (buf), "%d", db_points[i]);
            cairo_move_to (w->crb, x0+rect_width*0.1,y0+rect_height - (rect_height * fraction)-3);
        }
        else if (i<8)
        {
            snprintf (buf, sizeof (buf), "%d", db_points[i]);
            cairo_move_to (w->crb, x0+rect_width*0.2,y0+rect_height - (rect_height * fraction)-3);
        }
        else
        {
            snprintf (buf, sizeof (buf), " %d", db_points[i]);
            cairo_move_to (w->crb, x0+rect_width*0.21,y0+rect_height - (rect_height * fraction)-3);
        }
        cairo_text_path (w->crb, buf);
        cairo_fill (w->crb);
    }

    cairo_set_source_rgb(w->crb, 0.6, 0.6, 0.6);
    cairo_set_line_width(w->crb, 1.0);
    cairo_stroke(w->crb);
}

void create_vertical_meter_image(Widget_t *w, int width, int height) {

    if (w->image) cairo_surface_destroy(w->image);
    w->image = NULL;
    w->image = cairo_surface_create_similar(w->surface, CAIRO_CONTENT_COLOR_ALPHA, width * 2, height);
    if (!w->image || cairo_surface_status(w->image) != CAIRO_STATUS_SUCCESS) {
        w->image = nullptr;
        return;
    }
    cairo_t *cri = cairo_create(w->image);
    if (cairo_status(cri) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cri);
        return;
    }

    cairo_rectangle(cri, 0, 0, width, height);
    cairo_set_source_rgb(w->crb, 0.16,0.16,0.18);
    cairo_fill(cri);
    cairo_rectangle(cri, width, 0, width, height);
    cairo_fill(cri);

    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.1, 0.5, 0.1, 0.3);
    cairo_pattern_add_color_stop_rgba(pat, 0.2, 0.4, 0.4, 0.1, 0.3);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.5, 0.0, 0.0, 0.3);

    roundrec(cri, 0, 0, width, height, 5.5);
    cairo_set_source_rgb(cri, 0.16,0.16,0.18);
    cairo_set_line_width(cri, 4.0);
    cairo_stroke_preserve(cri); 

    cairo_set_source(cri, pat);
    cairo_fill(cri);
    cairo_pattern_destroy(pat);
    cairo_new_path(cri);

    pat = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.1, 0.5, 0.1, 1.0);
    cairo_pattern_add_color_stop_rgba(pat, 0.2, 0.4, 0.4, 0.1, 1.0);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.5, 0.0, 0.0, 1.0);

    roundrec(cri, width, 0, width, height, 5.5);

    cairo_set_source(cri, pat);
    cairo_fill_preserve(cri);
    cairo_set_source_rgb(cri, 0.14,0.14,0.16);
    cairo_set_line_width(cri, 4.0);
    cairo_stroke(cri); 
    cairo_pattern_destroy(pat);

    cairo_destroy(cri);
}

void draw_vmeter(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;

    int width, height;
    os_get_surface_size(w->image, &width, &height);
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    int width_t = metrics.width;
    int height_t = metrics.height;
    if (width != width_t*2 || height != height_t) {
        create_vertical_meter_image(w, width_t, height_t);
        os_get_surface_size(w->image, &width, &height);
    }
    double meterstate = log_meter(adj_get_value(w->adj_y));
    double oldstate = log_meter(w->adj_y->start_value);
    cairo_set_source_surface (w->crb, w->image, 0, 0);
    cairo_rectangle(w->crb,0, 0, width/2, height);
    cairo_fill(w->crb);
    cairo_set_source_surface (w->crb, w->image, -width/2, 0);
    cairo_rectangle(w->crb, 0, height, width/2, -height*meterstate);
    cairo_fill(w->crb);

    cairo_rectangle(w->crb, 0, height-height*oldstate, width/2, 3);
    cairo_fill(w->crb);
}

float power2dB(Widget_t *w, float power) {
    const float falloff = 27 * 60 * 0.0005;
    const float fallsoft = 6 * 60 * 0.0005;
    //power = 20.*log10(power);
    if (power <=  20.*log10(0.00021)) { // -70db
        power = 20.*log10(0.00000000001); //-137db
        w->adj->start_value = min(0.0,w->adj->start_value - fallsoft);
    }
    // retrieve old meter value and consider falloff
    if (power < w->adj->std_value) {
        power = max(power, w->adj->std_value - falloff);
        w->adj->start_value = min(0.0,w->adj->start_value - fallsoft);
    }
    if (power > w->adj->start_value) {
        w->adj->start_value = power ;
    }
    
    w->adj->std_value = power;
    return  power;
}

Widget_t* add_my_vmeter(Widget_t *parent, const char * label, bool show_scale,
                int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    create_vertical_meter_image(wid, width, height);
    wid->label = label;
    wid->adj_y = add_adjustment(wid,-70.0, -70.0, -180.0, 6.0,0.001, CL_METER);
    wid->adj = wid->adj_y;
    wid->flags &= ~USE_TRANSPARENCY;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_vmeter;
    if (show_scale) {
        Widget_t *wid2 = create_widget(parent->app, parent, x+width, y, width+4, height);
        wid2->scale.gravity = ASPECT;
        wid2->func.expose_callback =draw_vmeter_scale;
    }
    return wid;
}

/****************************************************************
 *    slider
****************************************************************/

void draw_vslider(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Metrics_t m;
    os_get_window_metrics(w, &m);
    if (!m.visible) return;

    float value = adj_get_state(w->adj_y);

    cairo_set_source_rgb(w->crb, 0.10, 0.11, 0.13);
    cairo_rectangle(w->crb, 0, 0, m.width, m.height);
    cairo_fill(w->crb);

    cairo_pattern_t *track = cairo_pattern_create_linear(0, 0, 0, m.height);
    cairo_pattern_add_color_stop_rgba(track, 0.0, 0.157, 0.165, 0.212, 0.8);
    cairo_pattern_add_color_stop_rgba(track, 1.0, 0.157, 0.165, 0.212, 1.0);

    cairo_rectangle(w->crb, m.width*0.25, 4, m.width*0.5, m.height-8);
    cairo_set_source(w->crb, track);
    cairo_fill(w->crb);
    cairo_pattern_destroy(track);

    float fill_h = (m.height-8) * value;
    float y = (m.height-4) - fill_h;

    cairo_pattern_t *fill = cairo_pattern_create_linear(0, y, 0, m.height);
    cairo_pattern_add_color_stop_rgba(fill, 0.0, 0.219,0.208,0.235, 0.8);
    cairo_pattern_add_color_stop_rgba(fill, 1.0, 0.16,0.16,0.18, 1.0);

    cairo_rectangle(w->crb, m.width*0.25, y, m.width*0.5, fill_h);
    cairo_set_source(w->crb, fill);
    cairo_fill(w->crb);
    cairo_pattern_destroy(fill);

    cairo_set_source_rgba(w->crb, 0.06,0.06,0.08, 0.6);
    cairo_set_line_width(w->crb, 2.2);
    cairo_move_to(w->crb, m.width*0.2, y);
    cairo_line_to(w->crb, m.width*0.8, y);
    cairo_stroke(w->crb);

    cairo_set_source_rgba(w->crb, 1,1,1,0.5);
    cairo_set_line_width(w->crb, 1.2);
    float fill_0 = (m.height-8) * 0.793103;
    float y0 = (m.height-4) - fill_0;
    cairo_move_to(w->crb, m.width*0.2, y0);
    cairo_line_to(w->crb, m.width*0.8, y0);
    cairo_stroke(w->crb);

    cairo_set_source_rgba(w->crb, 1,1,1,0.06);
    cairo_rectangle(w->crb, m.width*0.25, 4, m.width*0.5, m.height-8);
    cairo_stroke(w->crb);

    cairo_set_source_rgba(w->crb, 0,0,0,0.6);
    cairo_rectangle(w->crb, 0, 0, m.width, m.height);
    cairo_stroke(w->crb);

    cairo_text_extents_t ext;
    cairo_set_source_rgba(w->crb, 0.85, 0.85, 0.9, 0.85);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", adj_get_value(w->adj));
    cairo_set_font_size(w->crb, m.width * 0.40);
    cairo_text_extents(w->crb, buf, &ext);
    cairo_move_to(w->crb, (m.width - ext.width)/2, ext.height + 2);
    cairo_text_path (w->crb, buf);
    cairo_fill (w->crb);
}

void slider_released(void *w_, void* button_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    expose_widget(w);
}

Widget_t* add_my_vslider(Widget_t *parent, const char * label,
                int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    Slider_t *slider;
    slider = (Slider_t*)malloc(sizeof(Slider_t));
    slider->frames = 101;
    wid->private_struct = slider;
    wid->flags |= HAS_MEM;
    wid->label = label;
    wid->adj_y = add_adjustment(wid,0.0, 0.0, 0.0, 1.0,0.01, CL_CONTINUOS);
    wid->adj = wid->adj_y;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_vslider;
    wid->func.enter_callback = os_transparent_draw;
    wid->func.leave_callback = os_transparent_draw;
    wid->func.button_release_callback = slider_released;
    wid->func.mem_free_callback = slider_mem_free;
    return wid;
}


/****************************************************************
 *    value display
****************************************************************/

void draw_my_valuedisplay(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    int width = metrics.width-2;
    int height = metrics.height-2;
    if (!metrics.visible) return;

    cairo_rectangle(w->crb,2.0, 2.0, width, height);

    if(w->state==0) {
        cairo_set_line_width(w->crb, 1.0);
        use_shadow_color_scheme(w, NORMAL_);
        cairo_fill_preserve(w->crb);
        use_frame_color_scheme(w, NORMAL_);
    } else if(w->state==1) {
        use_shadow_color_scheme(w, PRELIGHT_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.5);
        use_frame_color_scheme(w, NORMAL_);
    } else if(w->state==2) {
        use_shadow_color_scheme(w, SELECTED_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, SELECTED_);
    } else if(w->state==3) {
        use_shadow_color_scheme(w, ACTIVE_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, ACTIVE_);
    } else if(w->state==4) {
        use_shadow_color_scheme(w, INSENSITIVE_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_frame_color_scheme(w, INSENSITIVE_);
    }
    cairo_stroke(w->crb); 

    cairo_rectangle(w->crb,4.0, 4.0, width, height);
    cairo_stroke(w->crb);
    cairo_rectangle(w->crb,3.0, 3.0, width, height);
    cairo_stroke(w->crb);

    cairo_text_extents_t extents;

    char s[64];
    float value = adj_get_value(w->adj);
    if (value > 10000.0f) {
        snprintf(s, 63,"%.1f k%s", value / 1000.0, w->input_label);
    } else if (value >= 1000.0f) {
        snprintf(s, 63, "%.2f k%s", value / 1000.0, w->input_label);
    } else  if (value >= 100.0f) {
        snprintf(s, 63, "%.1f %s", value, w->input_label);
    } else {
        snprintf(s, 63, "%.2f %s", value, w->input_label);
    }
    //if(strlen(w->label)) memcpy(s + strlen(s), w->label, strlen(w->label) + 1);
    
    use_text_color_scheme(w, get_color_state(w));
    float font_size = w->app->normal_font/w->scale.ascale;
    adjust_eq_font_size (w->crb, font_size, width, s);
    cairo_text_extents(w->crb,s , &extents);
    cairo_move_to (w->crb, (width-extents.width)*0.5, (height+extents.height)*0.55);
    cairo_text_path (w->crb, s);
    cairo_fill (w->crb);
    cairo_new_path (w->crb);

}

Widget_t* add_my_valuedisplay(Widget_t *parent, const char * label,
                const char * type, int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    wid->label = label;
    snprintf(wid->input_label, 31, "%s", type);
    wid->adj_y = add_adjustment(wid,0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
    wid->adj = wid->adj_y;
    wid->scale.gravity = CENTER;
    wid->func.enter_callback = os_transparent_draw;
    wid->func.leave_callback = os_transparent_draw;
    wid->func.expose_callback = draw_my_valuedisplay;
    return wid;
}


#ifdef __cplusplus
}
#endif
