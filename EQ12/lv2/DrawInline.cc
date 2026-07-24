
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

static float x_to_freq(float x, float f_min, float f_max, int width) {
    const float x_pad = 3.0f;
    float norm = (x - x_pad) / (width - 2.0f * x_pad);
    norm = clampf(norm, 0.0f, 1.0f);
    return f_min * powf(f_max / f_min, norm);
}

static float hermite_lookup(const float* v, int size, float index) {
    int i1 = (int)index;
    float t = index - i1;

    i1 = std::max(0, std::min(i1, size - 1));

    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, size - 1);
    int i3 = std::min(i1 + 2, size - 1);

    float p0 = v[i0];
    float p1 = v[i1];
    float p2 = v[i2];
    float p3 = v[i3];

    float c0 = p1;
    float c1 = 0.5f * (p2 - p0);
    float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);

    return ((c3 * t + c2) * t + c1) * t + c0;
}

static float bspline_lookup(const float* v, int size, float index) {
    int i1 = (int)index;
    float t = index - i1;
    i1 = std::max(0, std::min(i1, size - 1));
    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, size - 1);
    int i3 = std::min(i1 + 2, size - 1);
    float p0 = v[i0], p1 = v[i1], p2 = v[i2], p3 = v[i3];

    float t2 = t * t, t3 = t2 * t;
    float b0 = (1 - 3*t + 3*t2 - t3) / 6.0f;
    float b1 = (4 - 6*t2 + 3*t3) / 6.0f;
    float b2 = (1 + 3*t + 3*t2 - 3*t3) / 6.0f;
    float b3 = t3 / 6.0f;

    return p0*b0 + p1*b1 + p2*b2 + p3*b3;
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

    for (int px = 3; px < width - 3; ++px) {
        float freq = x_to_freq((float)px, f_min, f_max, width);
        float bin = freq * fft_size / sample_rate;
        if (bin < 1.0f) continue;
        if (bin > bins - 3) break;
        float db = hermite_lookup(mags, bins, bin);
        float y  = db_to_y(db, db_min, db_max, height);

        if (!started) {
            cairo_move_to(cr, px, y);
            started = 1;
        } else {
            cairo_line_to(cr, px, y);
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
    const float* ir_curve = ir.data();
    bins = ir.size();
    fft_size = bins * 2;

    for (int px = 3; px < width - 3; ++px) {
        float freq = x_to_freq((float)px, f_min, f_max, width);
        float bin = freq * fft_size / sample_rate;
        if (bin < 1.0f) continue;
        if (bin > bins - 3) break;
        float db = bspline_lookup(ir_curve, bins, bin);
        float y  = db_to_y(db, db_min, db_max, height);

        if (!started) {
            cairo_move_to(cr, px, y);
            started = true;
        } else {
            cairo_line_to(cr, px, y);
        }
    }

    cairo_stroke(cr);

}
