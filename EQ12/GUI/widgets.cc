
/*
 * widgets.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#include "xwidgets.h"
#include "xfile-dialog.h"
#include "tooltip.cc"

#ifdef __cplusplus
extern "C" {
#endif


/****************************************************************
 *    helpers
****************************************************************/

char* utf8crop(char* dst, const char* src, size_t sizeDest ) {
    if( sizeDest ){
        size_t sizeSrc = strlen(src);
        while( sizeSrc >= sizeDest ){
            const char* lastByte = src + sizeSrc;
            while( lastByte-- > src )
                if((*lastByte & 0xC0) != 0x80)
                    break;
            sizeSrc = lastByte - src;
        }
        memcpy(dst, src, sizeSrc);
        dst[sizeSrc] = '\0';
    }
    return dst;
}

void utf8crop_middle(char* dst, const char* src, size_t maxLen) {
    size_t len = strlen(src);
    if (len < maxLen) {
        strcpy(dst, src);
        return;
    }

    if (maxLen < 5) {
        utf8crop(dst, src, maxLen);
        return;
    }

    size_t left = (maxLen - 3) / 6;
    size_t right = maxLen - 3 - left;
    char tmp[256];
    utf8crop(tmp, src, left + 1);
    strcpy(dst, tmp);
    strcat(dst, "...");
    const char* tail = src + len - right;
    utf8crop(tmp, tail, right + 1);
    strcat(dst, tmp);
}

void adjust_font_size(cairo_t* cr, double font_size, int available_width, const char* label) {
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

static void null_callback(void *w_, void *user_data) {
    
}

static void dummy_callback(void *w_, void *button, void *user_data) {
    
}

/****************************************************************
 *    draw direction arrow
****************************************************************/

// dir_up != 0 -> points up (Load), dir_up == 0 -> points down (Save)
static void draw_direction_arrow(cairo_t *cr, double x, double y, double size,
                                  int state, const int active, int dir_up) {
    double pad = state ? size * 0.1 : size * 0.16;
    double offset = 0.0f;
    if (state == 2) offset = 1.0f; // pressed
    double x0 = x + pad + offset;
    double x1 = x + size - pad - offset;
    double y_top = y + pad + offset;
    double y_bot = y + size - pad - offset;
    double w = x1 - x0;
    double h = y_bot - y_top;

    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
    if (state) cairo_set_source_rgb(cr, 0.91, 0.949, 0.883);
    cairo_set_line_width(cr, size * 0.04);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    double ax = x0 + w * 0.83;
    double y_stem_a = dir_up ? (y_bot - h * 0.10) : (y_top + h * 0.15);
    double y_stem_b = dir_up ? (y_top + h * 0.15) : (y_bot - h * 0.10);
    double y_tip    = dir_up ? (y_top + h * 0.10) : (y_bot - h * 0.05);
    double y_wing   = dir_up ? (y_top + h * 0.32) : (y_bot - h * 0.32);

    cairo_move_to(cr, ax, y_stem_a);
    cairo_line_to(cr, ax, y_stem_b);
    cairo_stroke(cr);

    cairo_move_to(cr, ax - w * 0.13, y_wing);
    cairo_line_to(cr, ax, y_tip);
    cairo_line_to(cr, ax + w * 0.13, y_wing);
    cairo_stroke(cr);
}

/****************************************************************
 *      draw APO buttons
****************************************************************/

static void draw_apo_body(cairo_t *cr, double x, double y, double size, int state, const int active) {
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

    cairo_set_line_width(cr, size * 0.035);
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

    // frequency-curve foreground element
    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
    cairo_set_line_width(cr, size * 0.04);
    if (state) cairo_set_source_rgb(cr, 0.91, 0.949, 0.883);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, x0 + w * 0.050, y_top + h * 0.694);
    cairo_curve_to(cr, x0 + w * 0.131, y_top + h * 0.694,
                        x0 + w * 0.131, y_top + h * 0.306,
                        x0 + w * 0.221, y_top + h * 0.306);
    cairo_curve_to(cr, x0 + w * 0.303, y_top + h * 0.306,
                        x0 + w * 0.303, y_top + h * 0.778,
                        x0 + w * 0.392, y_top + h * 0.778);
    cairo_curve_to(cr, x0 + w * 0.473, y_top + h * 0.778,
                        x0 + w * 0.473, y_top + h * 0.194,
                        x0 + w * 0.555, y_top + h * 0.194);
    cairo_curve_to(cr, x0 + w * 0.587, y_top + h * 0.194,
                        x0 + w * 0.608, y_top + h * 0.222,
                        x0 + w * 0.620, y_top + h * 0.250);
    cairo_stroke(cr);
}

void draw_load_apo(cairo_t *cr, double x, double y, double size, int state, const int active) {
    cairo_save(cr);
    draw_apo_body(cr, x, y, size, state, active);
    draw_direction_arrow(cr, x, y, size, state, active, /*dir_up=*/1);
    cairo_restore(cr);
}

void draw_save_apo(cairo_t *cr, double x, double y, double size, int state, const int active) {
    cairo_save(cr);
    draw_apo_body(cr, x, y, size, state, active);
    draw_direction_arrow(cr, x, y, size, state, active, /*dir_up=*/0);
    cairo_restore(cr);
}

void draw_apo_load_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    draw_load_apo(w->crb, 0.0, 0.0, height, w->state, state);
    tooltip_set_my_text(w, "Load APO EQ config file");
}

void draw_apo_save_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    draw_save_apo(w->crb, 0.0, 0.0, height, w->state, state);
    tooltip_set_my_text(w, "Save as APO EQ config file");
}

/****************************************************************
 *      draw IR buttons
****************************************************************/

void draw_ir(cairo_t *cr, double x, double y, double size, int state, const int active) {
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

    cairo_set_line_width(cr, size * 0.035);
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

    // save-tray line (background element)
    cairo_set_line_width(cr, size * 0.035);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.35);
    if (state) cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.55);
    cairo_move_to(cr, x0, y_bot);
    cairo_line_to(cr, x1, y_bot);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
    cairo_set_line_width(cr, size * 0.04);
    if (state) cairo_set_source_rgb(cr, 0.91, 0.949, 0.883);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    // impulse response waveform
    double base = y_top + h * 0.55;
    cairo_move_to(cr, x0, base);
    cairo_line_to(cr, x0 + w * 0.16, base);
    cairo_line_to(cr, x0 + w * 0.28, y_top);
    cairo_line_to(cr, x0 + w * 0.40, base + h * 0.20);
    cairo_line_to(cr, x0 + w * 0.50, base - h * 0.08);
    cairo_line_to(cr, x0 + w * 0.58, base);
    cairo_stroke(cr);

    cairo_restore(cr);
}

void draw_ir_save_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    draw_ir(w->crb, 0.0, 0.0, height, w->state, state);
    draw_direction_arrow(w->crb, 0.0, 0.0, height, w->state, state, 0);
    tooltip_set_my_text(w, "Save as IR file");
}

void draw_ir_load_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;

    const int height = metrics.height;
    const int state  = (int)adj_get_value(w->adj); // 0 = off, 1 = on
    draw_ir(w->crb, 0.0, 0.0, height, w->state, state);
    draw_direction_arrow(w->crb, 0.0, 0.0, height, w->state, state, 1);
    tooltip_set_my_text(w, "Load IR file");
}

/****************************************************************
 *      file load button 
****************************************************************/

static void my_fdialog_response(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    FileButton *filebutton = (FileButton *)w->private_struct;
    if(user_data !=NULL) {
        char *tmp = strdup(*(const char**)user_data);
        free(filebutton->last_path);
        filebutton->last_path = NULL;
        filebutton->last_path = strdup(dirname(tmp));
        filebutton->path = filebutton->last_path;
        free(tmp);
    }
    w->func.user_callback(w,user_data);
    filebutton->is_active = false;
    adj_set_value(w->adj,0.0);
}

static void my_fbutton_callback(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    FileButton *filebutton = (FileButton *)w->private_struct;
    if (w->flags & HAS_POINTER && adj_get_value(w->adj)){
        filebutton->is_active = true;
        if (!filebutton->w) {
            filebutton->w = open_file_dialog(w,filebutton->path,filebutton->filter);
            filebutton->w->flags |= HIDE_ON_DELETE;
            widget_set_title(filebutton->w, w->label);
#ifdef _OS_UNIX_
            Atom wmStateAbove = XInternAtom(w->app->dpy, "_NET_WM_STATE_ABOVE", 1 );
            Atom wmNetWmState = XInternAtom(w->app->dpy, "_NET_WM_STATE", 1 );
            XChangeProperty(w->app->dpy, filebutton->w->widget, wmNetWmState, XA_ATOM, 32, 
                PropModeReplace, (unsigned char *) &wmStateAbove, 1); 
#elif defined _WIN32
            os_set_transient_for_hint(w, filebutton->w);
#endif
        } else {
            widget_show_all(filebutton->w);
        }
    } else if (w->flags & HAS_POINTER && !adj_get_value(w->adj)){
        if(filebutton->is_active)
            widget_hide(filebutton->w);
    }
}

static void my_fbutton_mem_free(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    FileButton *filebutton = (FileButton *)w->private_struct;
    free(filebutton->last_path);
    filebutton->last_path = NULL;
    free(filebutton);
    filebutton = NULL;
}

Widget_t *add_my_file_button(Widget_t *parent, int x, int y, int width, int height,
                           const char* label, const char *path, const char *filter) {
    FileButton *filebutton = (FileButton*)malloc(sizeof(FileButton));
    filebutton->path = path;
    filebutton->filter = filter;
    filebutton->last_path = NULL;
    filebutton->w = NULL;
    filebutton->is_active = false;
    Widget_t *fbutton = add_toggle_button(parent, label, x, y, width, height);
    fbutton->private_struct = filebutton;
    fbutton->label = "File Selector - Select APO File";
    fbutton->flags |= HAS_MEM;
    fbutton->scale.gravity = ASPECT;
    fbutton->func.mem_free_callback = my_fbutton_mem_free;
    fbutton->func.value_changed_callback = my_fbutton_callback;
    fbutton->func.dialog_callback = my_fdialog_response;
    fbutton->func.expose_callback = draw_apo_load_button;
    return fbutton;
}


Widget_t *add_my_lfile_button(Widget_t *parent, int x, int y, int width, int height,
                           const char* label, const char *path, const char *filter) {
    FileButton *filebutton = (FileButton*)malloc(sizeof(FileButton));
    filebutton->path = path;
    filebutton->filter = filter;
    filebutton->last_path = NULL;
    filebutton->w = NULL;
    filebutton->is_active = false;
    Widget_t *fbutton = add_toggle_button(parent, label, x, y, width, height);
    fbutton->private_struct = filebutton;
    fbutton->label = "File Selector - Select IR File";
    fbutton->flags |= HAS_MEM;
    fbutton->scale.gravity = ASPECT;
    fbutton->func.mem_free_callback = my_fbutton_mem_free;
    fbutton->func.value_changed_callback = my_fbutton_callback;
    fbutton->func.dialog_callback = my_fdialog_response;
    fbutton->func.expose_callback = draw_ir_load_button;
    return fbutton;
}

/****************************************************************
 *      file save button 
****************************************************************/

static void fxdialog_response(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    FileButton *filebutton = (FileButton *)w->private_struct;
    if(user_data !=NULL) {
        char *tmp = strdup(*(const char**)user_data);
        free(filebutton->last_path);
        filebutton->last_path = NULL;
        filebutton->last_path = strdup(dirname(tmp));
        filebutton->path = filebutton->last_path;
        free(tmp);
    }
    w->func.user_callback(w,user_data);
    filebutton->is_active = false;
    adj_set_value(w->adj,0.0);
}

static void fxbutton_callback(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    FileButton *filebutton = (FileButton *)w->private_struct;
    if (w->flags & HAS_POINTER && adj_get_value(w->adj)){
        filebutton->w = save_file_dialog(w,filebutton->path,filebutton->filter);
#ifdef _OS_UNIX_
        Atom wmStateAbove = XInternAtom(w->app->dpy, "_NET_WM_STATE_ABOVE", 1 );
        Atom wmNetWmState = XInternAtom(w->app->dpy, "_NET_WM_STATE", 1 );
        XChangeProperty(w->app->dpy, filebutton->w->widget, wmNetWmState, XA_ATOM, 32, 
            PropModeReplace, (unsigned char *) &wmStateAbove, 1); 
#elif defined _WIN32
        os_set_transient_for_hint(w, filebutton->w);
#endif
        filebutton->is_active = true;
    }
}

static void fxbutton_mem_free(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    FileButton *filebutton = (FileButton *)w->private_struct;
    free(filebutton->last_path);
    filebutton->last_path = NULL;
    free(filebutton);
    filebutton = NULL;
}

Widget_t *add_xsave_file_button(Widget_t *parent, int x, int y, int width, int height,
                           const char *label, const char *path, const char *filter) {
    FileButton *filebutton = (FileButton*)malloc(sizeof(FileButton));
    filebutton->path = path;
    filebutton->filter = filter;
    filebutton->last_path = NULL;
    filebutton->w = NULL;
    filebutton->is_active = false;
    Widget_t *fbutton = add_toggle_button(parent, label, x, y, width, height);
    fbutton->private_struct = filebutton;
    fbutton->flags |= HAS_MEM;
    fbutton->flags |= NO_PROPAGATE;
    fbutton->scale.gravity = ASPECT;
    fbutton->func.expose_callback = draw_ir_save_button;
    fbutton->func.mem_free_callback = fxbutton_mem_free;
    fbutton->func.value_changed_callback = fxbutton_callback;
    fbutton->func.dialog_callback = fxdialog_response;
    return fbutton;
}

Widget_t *add_ysave_file_button(Widget_t *parent, int x, int y, int width, int height,
                           const char *label, const char *path, const char *filter) {
    FileButton *filebutton = (FileButton*)malloc(sizeof(FileButton));
    filebutton->path = path;
    filebutton->filter = filter;
    filebutton->last_path = NULL;
    filebutton->w = NULL;
    filebutton->is_active = false;
    Widget_t *fbutton = add_toggle_button(parent, label, x, y, width, height);
    fbutton->private_struct = filebutton;
    fbutton->flags |= HAS_MEM;
    fbutton->flags |= NO_PROPAGATE;
    fbutton->scale.gravity = ASPECT;
    fbutton->func.expose_callback = draw_apo_save_button;
    fbutton->func.mem_free_callback = fxbutton_mem_free;
    fbutton->func.value_changed_callback = fxbutton_callback;
    fbutton->func.dialog_callback = fxdialog_response;
    return fbutton;
}

/****************************************************************
 *      simple text button 
****************************************************************/

void draw_i_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int width = metrics.width;
    int height = metrics.height;
    float offset = 0.0;
    float g = 0.0;
    if(w->state==1 && ! (int)w->adj_y->value) {
        offset = 2.0;
    } else if(w->state==1) {
        offset = 3.0;
    } else if(w->state==2) {
        offset = 3.0;
    } else if(w->state==3) {
        offset = 2.0;
    }
    if(w->adj->value) g = -0.5;
    widget_set_scale(w);
    cairo_text_extents_t extents_f;
    adjust_font_size (w->crb, (w->app->normal_font + 1 + offset) * w->app->hdpi, width, w->label);
    cairo_set_source_rgb(w->crb, 0.91, 0.949 + g, 0.883 + g);
    cairo_text_extents(w->crb, w->label, &extents_f);
    cairo_move_to (w->crb, (width*0.5)-(extents_f.width/2), height-(extents_f.height/4));
    cairo_text_path (w->crb, w->label);
    cairo_fill (w->crb);
    widget_reset_scale(w);
    
}

Widget_t *add_my_button(Widget_t *parent, int x, int y, int width, int height, const char *label) {
    Widget_t *fbutton = add_button(parent, label, x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    //fbutton->flags |= NO_PROPAGATE;
    fbutton->func.expose_callback = draw_i_button;
    return fbutton;
}

Widget_t *add_my_toggle_button(Widget_t *parent, int x, int y, int width, int height, const char *label) {
    Widget_t *fbutton = add_toggle_button(parent, label, x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE;
    fbutton->func.expose_callback = draw_i_button;
    return fbutton;
}

/****************************************************************
 *      label
****************************************************************/

void draw_label(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int width = metrics.width;
    int height = metrics.height;
    char label[124];
    memset(label, '\0', sizeof(char)*124);
    utf8crop_middle(label, w->label, 39);
    cairo_text_extents_t extents_f;
    widget_set_scale(w);
    adjust_font_size(w->crb, w->app->normal_font * w->app->hdpi, width, label);
    cairo_set_source_rgb(w->crb, 0.91, 0.949, 0.883);
    cairo_text_extents(w->crb, label, &extents_f);
    cairo_move_to (w->crb, (width*0.5)-(extents_f.width/2), height-(extents_f.height/4));
    cairo_text_path (w->crb, label);
    cairo_fill (w->crb);
    widget_reset_scale(w);
}    

Widget_t* add_my_label(Widget_t *parent, const char * label,
                        int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    wid->label = label;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_label;
    return wid;
}

/****************************************************************
 *      combobox
****************************************************************/

static void draw_my_combobox(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    int width = metrics.width;
    int height = metrics.height;
    int v = (int)adj_get_value(w->adj);
    int vl = v - (int) w->adj->min_value;
   // if (v<0) return;
    Widget_t * menu = w->childlist->childs[1];
    Widget_t* view_port =  menu->childlist->childs[0];
    ComboBox_t *comboboxlist = (ComboBox_t*)view_port->parent_struct;

    char label[32];
    memset(label, '\0', sizeof(char)*32);
    cairo_text_extents_t extents_f;
    adjust_font_size (w->crb, w->app->normal_font * w->app->hdpi, width, label);
    if (w->state) adjust_font_size (w->crb, (w->app->normal_font+2) * w->app->hdpi, width, label);
    widget_set_scale(w);
    strcpy(label, comboboxlist->list_names[vl]);
    use_text_color_scheme(w, get_color_state(w));
    cairo_text_extents(w->crb, label, &extents_f);
    cairo_move_to (w->crb, (width*0.5)-(extents_f.width/2), height-(extents_f.height/4));
    cairo_text_path (w->crb, label);
    cairo_fill (w->crb);
    widget_reset_scale(w);

}

Widget_t* add_my_combobox(Widget_t *p,const char * label,
                                int x, int y, int width, int height) {
    Widget_t* w = add_combobox(p, label, x, y, width, height);
    w->scale.gravity = ASPECT;
    w->func.expose_callback = draw_my_combobox;
    w->childlist->childs[0]->func.expose_callback = null_callback;
    w->childlist->childs[0]->func.button_release_callback = dummy_callback;
    destroy_widget(w->childlist->childs[0], w->app);
    //w->func.value_changed_callback = value_changed;
    return w;
}


#ifdef __cplusplus
}
#endif
