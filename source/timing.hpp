#include "util.hpp"
#include "graphics.hpp"

void timing_start(char *label);
void timing_finish_frame();
void timing_draw(DrawTargetSlice *canvas);

#if 1

void timing_start(char *label)
{}

void timing_finish_frame()
{}

void timing_draw(DrawTargetSlice *canvas)
{}

#else

enum {
    TIMING_FRAMES = 64,
    TIMING_ENTRIES_PER_FRAME = 8,
};

struct TimingEntry {
    Time t;
    char *label;
};

struct TimingFrame
{
    TimingEntry entries[TIMING_ENTRIES_PER_FRAME];
    s32 entries_len;
};

struct Timings
{
    TimingFrame frames[TIMING_FRAMES];
    s32 current;
};

global_variable Timings timings;

void timing_start(char *label)
{
    Time now = time_read();

    TimingFrame *frame = &timings.frames[timings.current];
    assert(frame->entries_len + 1 <= TIMING_ENTRIES_PER_FRAME);
    frame->entries[frame->entries_len++] = { now, label };
}

void timing_finish_frame()
{
    timing_start(null);
    timings.current = (timings.current + 1) % TIMING_FRAMES;
    timings.frames[timings.current] = {};
}

/*
    hue is an angle in degrees (can be any value, gets normalized to 0..365)
    saturation and value are in 0..256
*/
static u32
hsv_to_rgb(s32 hue, u8 saturation, u8 value)
{
    hue %= 360;
    if (hue < 0) hue = 360 + hue;

    s32 ci = ((s32) saturation) * ((s32) value);
    s32 xi = ci * (60 - abs(hue%120 - 60));
    xi = xi / (256*60);
    s32 mi = (((s32) value)*256 - ci) / 256;

    u8 m = (u8) mi;
    u8 c = (u8) clamp(ci/256 + mi, 0, 256);
    u8 x = (u8) clamp(xi + mi, 0, 256);

    u8 r, g, b;

    if (hue < 60)       { r = c; g = x; b = m; }
    else if (hue < 120) { r = x; g = c; b = m; }
    else if (hue < 180) { r = m; g = c; b = x; }
    else if (hue < 240) { r = m; g = x; b = c; }
    else if (hue < 300) { r = x; g = m; b = c; }
    else                { r = c; g = m; b = x; }

    u32 color = (((u32) r) << 16) | (((u32) g) << 8) | ((u32) b);
    return(color);
}

void timing_draw(DrawTargetSlice *canvas)
{
    enum {
        DX = 5,
        DY_MS = 6,
        MS_TARGET = 16,
        MARGIN = 50,
    };
    static const
    uint32_t COLORS[] = { 0xc555a0, 0xeb2428, 0xffe444, 0xa6cB3a, 0x23a9e1 };

    s32 width = canvas->area.x1 - canvas->area.x0;
    s32 height = canvas->area.y1 - canvas->area.y0;

    s32 x1 = width - MARGIN;
    s32 x0 = x1 - TIMING_FRAMES*DX;
    s32 y1 = height - MARGIN;
    s32 y0 = y1 - DY_MS*MS_TARGET;

    struct {
        u32 hash;
        char *text;
    } labels[64];
    s32 labels_len = 0;

    for (s32 fi = 0; fi < TIMING_FRAMES; ++fi) {
        s32 x = x0 + fi*DX;
        s32 y = y1;

        for (s32 pi = 0; pi < timings.frames[fi].entries_len - 1; ++pi) {
            s32 dy = time_convert(timings.frames[fi].entries[pi].t, timings.frames[fi].entries[pi + 1].t, DY_MS*MILLISECONDS);
            char *label = timings.frames[fi].entries[pi].label;
            u32 label_hash = hash_fnv1a(label);
            u32 color = hsv_to_rgb(label_hash, 200, 180);
            draw_solid(canvas, x, y - dy, x + DX - 1, y, color);
            y -= dy;

            bool label_found = false;
            for (s32 li = 0; li < labels_len; ++li) {
                if (labels[li].hash == label_hash) {
                    label_found = true;
                }
            }
            if (!label_found && labels_len < alen(labels)) {
                labels[labels_len++] = { label_hash, label };
            }
        }
    }

    for (s32 li = 0; li < labels_len; ++li) {
    }

    draw_solid(canvas, x0, y1, x1, y1 + 1, 0x888888);
    draw_solid(canvas, x0 - 1, y0 - 1, x0, y1 + 1, 0x888888);
    draw_solid(canvas, x1, y0 - 1, x1 + 1, y1 + 1, 0x888888);
    draw_solid(canvas, x0, y0 - 1, x1, y0, 0xff0000);
}

#endif