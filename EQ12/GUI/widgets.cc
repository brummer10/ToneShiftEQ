
/*
 * widgets.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#include "xwidgets.h"
#include "xfile-dialog.h"


#ifdef __cplusplus
extern "C" {
#endif


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
    cairo_set_font_size (w->crb, w->app->normal_font * w->app->hdpi);
    cairo_set_source_rgb(w->crb, 0.91, 0.949, 0.883);
    cairo_text_extents(w->crb, label, &extents_f);
    cairo_move_to (w->crb, (width*0.5)-(extents_f.width/2), height-(extents_f.height/4));
    cairo_text_path (w->crb, label);
    cairo_fill (w->crb);
    widget_reset_scale(w);
}    


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
            widget_set_title(filebutton->w, _("File Selector - Select WAV File"));
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
    if(w->adj_y->value) g = -0.5;
    widget_set_scale(w);
    cairo_text_extents_t extents_f;
    cairo_set_font_size (w->crb, (w->app->normal_font + 1 + offset) * w->app->hdpi);
    cairo_set_source_rgb(w->crb, 0.91, 0.949 + g, 0.883 + g);
    cairo_text_extents(w->crb, w->label, &extents_f);
    cairo_move_to (w->crb, (width*0.5)-(extents_f.width/2), height-(extents_f.height/4));
    cairo_text_path (w->crb, w->label);
    cairo_fill (w->crb);
    widget_reset_scale(w);
    
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
    fbutton->flags |= HAS_MEM;
    fbutton->scale.gravity = ASPECT;
    fbutton->func.mem_free_callback = my_fbutton_mem_free;
    fbutton->func.value_changed_callback = my_fbutton_callback;
    fbutton->func.dialog_callback = my_fdialog_response;
    fbutton->func.expose_callback = draw_i_button;
    return fbutton;
}

Widget_t* add_my_label(Widget_t *parent, const char * label,
                        int x, int y, int width, int height) {

    Widget_t *wid = create_widget(parent->app, parent, x, y, width, height);
    wid->label = label;
    wid->scale.gravity = ASPECT;
    wid->func.expose_callback = draw_label;
    return wid;
}


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
    fbutton->func.expose_callback = draw_i_button;
    fbutton->func.mem_free_callback = fxbutton_mem_free;
    fbutton->func.value_changed_callback = fxbutton_callback;
    fbutton->func.dialog_callback = fxdialog_response;
    return fbutton;
}

Widget_t *add_my_button(Widget_t *parent, int x, int y, int width, int height, const char *label) {
    Widget_t *fbutton = add_button(parent, label, x, y, width, height);
    fbutton->scale.gravity = ASPECT;
    fbutton->flags |= NO_PROPAGATE;
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


static void null_callback(void *w_, void *user_data) {
    
}

static void dummy_callback(void *w_, void *button, void *user_data) {
    
}

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
    cairo_set_font_size (w->crb, w->app->normal_font * w->app->hdpi);
    if (w->state) cairo_set_font_size (w->crb, (w->app->normal_font+2) * w->app->hdpi);
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
    w->func.expose_callback = draw_my_combobox;
    w->childlist->childs[0]->func.expose_callback = null_callback;
    w->childlist->childs[0]->func.button_release_callback = dummy_callback;
    //destroy_widget(w->childlist->childs[0], w->app);
    //w->func.value_changed_callback = value_changed;
    return w;
}


#ifdef __cplusplus
}
#endif
