
/*
 * DrawInline.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        DrawInline.cc - part of ToneShiftEQ.cpp 
                        draw the LV2 Inline Display
****************************************************************/


static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

static float db_to_y(float db, float db_min, float db_max, int height) {
    float norm = (db - db_min) / (db_max - db_min);
    norm = clampf(norm, 0.0f, 1.0f);
    return (1.0f - norm) * height;
}

static float freq_to_x(float freq, float f_min, float f_max, int width) {
    const float x_pad = 3.0f;
    const float inv_log_range = 1.0f / log10f(f_max / f_min);
    if (freq < f_min) freq = f_min;
    if (freq > f_max) freq = f_max;
    float norm = log10f(freq / f_min) * inv_log_range;
    return  x_pad + norm * (width - 2.0f * x_pad);
}

static void draw_inline(Xtoneshifteq *self , cairo_t* cr) {

    const std::vector<float> ir = self->getIRMag();
    const float* mags = self->getMagnitudes();
    int bins = self->getBins();
    float sample_rate =  self->sampleRate;
    int fft_size = bins * 2; // 2048;

    int width  = self->width;
    int height = self->height;

    const float f_min = 20.0f;
    const float f_max = 20000.0f;

    const float db_min = -72.0f;
    const float db_max = 24.0f;

    // Background
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.011);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    // Grid: Frequencies
    cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
    cairo_set_line_width(cr, 1.0);

    float freqs[] = {20, 100, 1000, 10000, 20000};
    int num_freqs = sizeof(freqs) / sizeof(freqs[0]);

    for (int i = 0; i < num_freqs; ++i) {
        float x = freq_to_x(freqs[i], f_min, f_max, width);
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
    }
    cairo_stroke(cr);

    // Grid: dB
    float db_lines[] = { -48, -24, -12, 0, 12, 24};
    int num_db = sizeof(db_lines) / sizeof(db_lines[0]);

    for (int i = 0; i < num_db; ++i) {
        float y = db_to_y(db_lines[i], db_min, db_max, height);
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    cairo_stroke(cr);

    // Spectrum Line
    cairo_pattern_t* lpat = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgba(lpat, 0.0, 0.75, 0.2, 0.9, 0.65);
    cairo_pattern_add_color_stop_rgba(lpat, 1.0, 0.45, 0.2, 0.75, 0.65);
    cairo_set_source(cr, lpat);
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    int started = 0;

    for (int i = 1; i < bins; ++i) {

        float freq = (float)i * sample_rate / fft_size;

        if (freq < f_min || freq > f_max)
            continue;

        float x = freq_to_x(freq, f_min, f_max, width);
        float db = mags[i] ;
        float y = db_to_y(db, db_min, db_max, height);

        if (!started) {
            float x0 = freq_to_x(f_min, f_min, f_max, width);
            float db0 = mags[1] ;
            float y0 = db_to_y(db0, db_min, db_max, height);
            cairo_move_to(cr, x0, y0);
            started = 1;
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    cairo_stroke_preserve(cr);
    cairo_pattern_destroy(lpat);

    // Spectrum fill
    if (started) {
        //cairo_set_source_rgba(cr,  0.17, 0.82, 0.64, 0.15);
        cairo_line_to(cr, width, height);
        cairo_line_to(cr, 3, height);
        cairo_close_path(cr);
        cairo_pattern_t* pat = cairo_pattern_create_linear(0, 0, 0, height);
        cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.75, 0.2, 0.9, 0.25);
        cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.45, 0.2, 0.75, 0.05);
        cairo_set_source(cr, pat);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);
    }

    cairo_set_source_rgba(cr, 0.545, 0.914, 0.992, 0.8);

    cairo_set_line_width(cr, 1);
    started = 0;
    const float* ir_ = ir.data();
    for (int i = 1; i < 4096; ++i) {

        float freq = (float)i * sample_rate / (4096*2);

        if (freq < f_min || freq > f_max)
            continue;

        float x = freq_to_x(freq, f_min, f_max, width);
        float db = ir_[i] ;
        float y = db_to_y(db, db_min, db_max, height);

        if (!started) {
            cairo_move_to(cr, x, y);
            started = 1;
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    cairo_stroke(cr);

}
