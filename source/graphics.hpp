#pragma once


struct Glyph
{
    s32 codepoint;
    u32 *data;
    s32 width, height;
    s32 offset_x;
    s32 offset_y;
};

struct FontMetrics
{
    s32 size;
    s32 advance;
    s32 line_height;
    s32 ascent;
    s32 descent;
};

struct FontBackend;

void _font_backend_change(FontBackend *backend, s32 size, FontMetrics *metrics);
bool _font_backend_make_glyph(FontBackend *backend, s32 codepoint, Glyph *glyph);

#if FONT_BACKEND_gdi
#include "font_gdi.hpp"
#endif

struct Font
{
    FontBackend backend;
    FontMetrics metrics;

    u32 ascii_glyph_map[4];
    Glyph ascii_glyphs[128];
    HashMap32 known_glyphs;
    Array<Glyph> available;

    struct DataBlock
    {
        DataBlock *previous;
        s32 length;
        u32 _padding;
        u32 data[1024*1024];
    };
    DataBlock *pixel_data;
    DataBlock *pixel_data_free;
};

void font_change_size(Font *font, s32 new_size);
void glyphs_for_codepoint(Font *font, DecodedCodepoint decoded, Glyph glyphs[8], s32 *glyph_count, bool *glyphs_are_escape_sequence);

struct DrawCommand
{
    s32 x0, y0;
    s32 x1, y1;
    enum {
        SOLID,
        MULTIPLY,
        GLYPH,
    } kind;

    u32 color;
    u32 *glyph_data;
};

struct DrawTarget
{
    s32 width, height;

    u32 *buffer;
    s32 buffer_capacity;

    enum { GRID_WIDTH = 6, GRID_HEIGHT = 8 };
    struct GridCell
    {
        Rect area;
        bool changed_this_frame;
        u32 current_hash;
    };
    GridCell grid[GRID_WIDTH * GRID_HEIGHT];

    DrawCommand *commands;
    s32 commands_length, commands_capacity;
};

struct DrawTargetSlice
{
    DrawTarget *target;
    Rect area;
};


void draw_target_finish(DrawTarget *target);
void draw_target_set_size(DrawTarget *target, s32 width, s32 height);

DrawTargetSlice draw_target_as_slice(DrawTarget *target);
DrawTargetSlice draw_target_slice(DrawTargetSlice *slice, s32 x0, s32 y0, s32 x1, s32 y1);

void draw_single_pixel(DrawTargetSlice *slice, s32 x, s32 y, u32 color);
void draw_solid(DrawTargetSlice *slice, s32 x0, s32 y0, s32 x1, s32 y1, u32 color);
void draw_blend(DrawTargetSlice *slice, s32 x0, s32 y0, s32 x1, s32 y1, u32 color);
void draw_glyph(DrawTargetSlice *slice, Font *font, Glyph glyph, s32 x, s32 y, u32 color);
s32 draw_glyphs(DrawTargetSlice *slice, Font *font, DecodedCodepoint decoded, s32 x, s32 y, u32 color, u32 escape_color = 0);
s32 draw_text_ellipsis(DrawTargetSlice *slice, Font *font, str text, s32 x0, s32 x1, s32 y, u32 color, bool align_right);


#define _DRAW_TARGET_HASH_INITIAL 2166136261
void _draw_target_hash(u32 *hash, u8 *data, s32 size)
{
    while (size--) *hash = (*hash ^ *data++) * 16777619;
}

void _draw_target_add_command(DrawTarget *target, DrawCommand command)
{
    if (target->commands_length + 1 > target->commands_capacity) {
        target->commands_capacity = next_power_of_two(target->commands_capacity + 128);
        target->commands = (DrawCommand *) heap_grow(target->commands, target->commands_capacity * sizeof(DrawCommand));
    }
    target->commands[target->commands_length++] = command;
}


void draw_target_set_size(DrawTarget *target, s32 width, s32 height)
{
    if (width != target->width || height != target->height) {
        target->width = width;
        target->height = height;
        if (width*height > target->buffer_capacity) {
            target->buffer_capacity = next_power_of_two(width*height);
            if (target->buffer) heap_free((void *) target->buffer);
            target->buffer = (u32 *) heap_alloc(target->buffer_capacity * sizeof(target->buffer[0]));
        }

        for (s32 gx = 0; gx < DrawTarget::GRID_WIDTH; ++gx) {
            for (s32 gy = 0; gy < DrawTarget::GRID_HEIGHT; ++gy) {
                s32 slot = gx + gy*DrawTarget::GRID_WIDTH;
                target->grid[slot].current_hash = _DRAW_TARGET_HASH_INITIAL;
                target->grid[slot].area.x0 = gx * target->width / DrawTarget::GRID_WIDTH;
                target->grid[slot].area.x1 = (gx + 1) * target->width / DrawTarget::GRID_WIDTH;
                target->grid[slot].area.y0 = gy * target->height / DrawTarget::GRID_HEIGHT;
                target->grid[slot].area.y1 = (gy + 1) * target->height / DrawTarget::GRID_HEIGHT;
            }
        }
    }
}


void _draw_target_do_command(DrawTarget *target, DrawCommand *command, Rect box)
{
    s32 x0 = max(command->x0, box.x0);
    s32 x1 = min(command->x1, box.x1);
    s32 y0 = max(command->y0, box.y0);
    s32 y1 = min(command->y1, box.y1);
    s32 row_width = x1 - x0;

    switch (command->kind) {
        case DrawCommand::SOLID: {
            u32 color = command->color;

            for (s32 y = y0; y < y1; ++y) {
                u32 *pixel = &target->buffer[y*target->width + x0];
                u32 *row_end = pixel + row_width;
                while (pixel < row_end) *pixel++ = color;
            }
        } break;

        case DrawCommand::MULTIPLY: {
            union Swizzle
            {
                u32 v;
                struct { u8 b, g, r, a; };
            } color_swizzle, pixel_swizzle;
            color_swizzle.v = command->color;

            int t1 = color_swizzle.a;
            int t2 = 255 - t1;

            for (s32 y = y0; y < y1; ++y) {
                u32 *pixel = &target->buffer[y*target->width + x0];
                u32 *row_end = pixel + row_width;
                while (pixel < row_end) {
                    pixel_swizzle.v = *pixel;
                    pixel_swizzle.r = (u8) (((((int) pixel_swizzle.r)*t2) + (((int)color_swizzle.r)*t1)) / 255);
                    pixel_swizzle.g = (u8) (((((int) pixel_swizzle.g)*t2) + (((int)color_swizzle.g)*t1)) / 255);
                    pixel_swizzle.b = (u8) (((((int) pixel_swizzle.b)*t2) + (((int)color_swizzle.b)*t1)) / 255);
                    *pixel = pixel_swizzle.v;
                    ++pixel;
                }
            }
        } break;

        case DrawCommand::GLYPH: {
            s32 src_stride = command->x1 - command->x0;

            u32 *src = command->glyph_data + (x0 - command->x0) + src_stride*(y0 - command->y0);
            u32 *src_end = src + src_stride*(y1 - y0);

            s32 dst_stride = target->width;
            u32 *dst = &target->buffer[y0*dst_stride + x0];

            __m128i foreground_word = _mm_unpacklo_epi8(_mm_set1_epi32(command->color), _mm_setzero_si128());

            debug_assert(src_stride%4 == 0);
            s32 simd_row_width = row_width/4*4;
            while (src < src_end) {
                u32 *src_pixel = src;
                u32 *dst_pixel = dst;

                /*
                NB (Morten, 2020-07-18)
                dst is the backbuffer, src is the glyph texture and color is command->color.
                We want to do the following linear blend:
                    dst' = dst*(1.0 - src) + color*src
                Because all values are represented as integers between 0 and 255 we in practice want to do:
                    dst' = ( dst*(255 - src) + color*src ) / 255
                Division by 255 is non-trivial in SSE, but division by 256 is. We therefore do:
                    dst' = ( dst*(256 - src) + color*src ) / 256
                Note that we pretend src is in [0, 256] when in reality it is in [0, 256[.
                When src = 0 we still properly get dst' = dst, which means we won't have visible artifacts around glyphs.
                When src = 255 we will however still get 1/256th of dst blended in.
                I suspect the error introduced by this will be quite hard to spot though.
                */

                u32 *src_pixel_end_simd = src + simd_row_width;
                u32 *src_pixel_end = src + row_width;
                while (src_pixel < src_pixel_end_simd) {
                    __m128i background = _mm_loadu_si128((__m128i *) dst_pixel);
                    __m128i factor = _mm_loadu_si128((__m128i *) src_pixel);

                    __m128i background01 = _mm_unpacklo_epi8(background, _mm_setzero_si128());
                    __m128i background23 = _mm_unpackhi_epi8(background, _mm_setzero_si128());
                    __m128i factor01 = _mm_unpacklo_epi8(factor, _mm_setzero_si128());
                    __m128i factor23 = _mm_unpackhi_epi8(factor, _mm_setzero_si128());
                    __m128i factor_inv01 = _mm_sub_epi16(_mm_set1_epi16(0x100), factor01);
                    __m128i factor_inv23 = _mm_sub_epi16(_mm_set1_epi16(0x100), factor23);
                    __m128i left01 = _mm_mullo_epi16(background01, factor_inv01);
                    __m128i left23 = _mm_mullo_epi16(background23, factor_inv23);
                    __m128i right01 = _mm_mullo_epi16(foreground_word, factor01);
                    __m128i right23 = _mm_mullo_epi16(foreground_word, factor23);
                    __m128i result01 = _mm_add_epi16(left01, right01);
                    __m128i result23 = _mm_add_epi16(left23, right23);
                    result01 = _mm_srli_epi16(result01, 8);
                    result23 = _mm_srli_epi16(result23, 8);
                    __m128i result = _mm_packus_epi16(result01, result23);

                    _mm_storeu_si128((__m128i *) dst_pixel, result);
                    dst_pixel += 4;
                    src_pixel += 4;
                }
                while (src_pixel < src_pixel_end) {
                    __m128i background = _mm_loadu_si32((__m128i *) dst_pixel);
                    __m128i factor = _mm_loadu_si32((__m128i *) src_pixel);

                    __m128i background01 = _mm_unpacklo_epi8(background, _mm_setzero_si128());
                    __m128i factor01 = _mm_unpacklo_epi8(factor, _mm_setzero_si128());
                    __m128i factor_inv01 = _mm_sub_epi16(_mm_set1_epi16(0x100), factor01);
                    __m128i left01 = _mm_mullo_epi16(background01, factor_inv01);
                    __m128i right01 = _mm_mullo_epi16(foreground_word, factor01);
                    __m128i result01 = _mm_add_epi16(left01, right01);
                    result01 = _mm_srli_epi16(result01, 8);
                    __m128i result = _mm_packus_epi16(result01, _mm_setzero_si128());

                    _mm_storeu_si32((__m128i *) dst_pixel, result);
                    dst_pixel += 1;
                    src_pixel += 1;
                }

                dst += dst_stride;
                src += src_stride;
            }
        } break;

        default: assert(false);
    }
}

void draw_target_finish(DrawTarget *target)
{
    u32 new_hashes[alen(target->grid)];
    for (s32 i = 0; i < alen(target->grid); ++i) {
        new_hashes[i] = _DRAW_TARGET_HASH_INITIAL;
        target->grid[i].changed_this_frame = false;
    }

    for (s32 i = 0; i < target->commands_length; ++i) {
        DrawCommand *command = &target->commands[i];
        s32 gx0 = command->x0*DrawTarget::GRID_WIDTH / target->width;
        s32 gx1 = (command->x1*DrawTarget::GRID_WIDTH - 1) / target->width;
        s32 gy0 = command->y0*DrawTarget::GRID_HEIGHT / target->height;
        s32 gy1 = (command->y1*DrawTarget::GRID_HEIGHT - 1) / target->height;

        for (s32 gx = gx0; gx <= gx1; ++gx) {
            for (s32 gy = gy0; gy <= gy1; ++gy) {
                _draw_target_hash(&new_hashes[gx + gy*DrawTarget::GRID_WIDTH], (u8 *) command, sizeof(DrawCommand));
            }
        }
    }

    for (s32 gx = 0; gx < DrawTarget::GRID_WIDTH; ++gx) {
        for (s32 gy = 0; gy < DrawTarget::GRID_HEIGHT; ++gy) {
            s32 slot = gx + gy*DrawTarget::GRID_WIDTH;
            DrawTarget::GridCell *cell = &target->grid[slot];

            cell->changed_this_frame = new_hashes[slot] != cell->current_hash;
            if (cell->changed_this_frame) {
                cell->current_hash = new_hashes[slot];

                for (s32 i = 0; i < target->commands_length; ++i) {
                    DrawCommand *command = &target->commands[i];
                    bool inside = command->x1 >= cell->area.x0 && command->x0 <= cell->area.x1 &&
                                  command->y1 >= cell->area.y0 && command->y0 <= cell->area.y1;
                    if (inside) {
                        _draw_target_do_command(target, command, cell->area);
                    }
                }
            }

            #if 0
            // For debugging, fade squares which didn't change to white
            else {
                union Swizzle
                {
                    u32 v;
                    struct { u8 b, g, r, a; };
                } swizzle;

                for (s32 y = y0; y < y1; ++y) {
                    u32 *pixel = &target->buffer[y*target->width + x0];
                    u32 *row_end = pixel + (x1 - x0);
                    while (pixel < row_end) {
                        swizzle.v = *pixel;
                        swizzle.r = min(swizzle.r + 3, 255);
                        swizzle.g = min(swizzle.g + 4, 255);
                        swizzle.b = min(swizzle.b + 3, 255);
                        *pixel = swizzle.v;
                        ++pixel;
                    }
                }
            }
            #endif
        }
    }

    target->commands_length = 0;
}

DrawTargetSlice draw_target_as_slice(DrawTarget *target)
{
    DrawTargetSlice slice = {0};
    slice.target = target;
    slice.area.x0 = 0;
    slice.area.y0 = 0;
    slice.area.x1 = target->width;
    slice.area.y1 = target->height;
    return(slice);
}

DrawTargetSlice draw_target_slice(DrawTargetSlice *slice, s32 x0, s32 y0, s32 x1, s32 y1)
{
    assert(x0 >= 0 && y0 >= 0 && x1 <= (slice->area.x1 - slice->area.x0) && y1 <= (slice->area.y1 - slice->area.y0));
    DrawTargetSlice subslice = {0};
    subslice.target = slice->target;
    subslice.area.x0 = slice->area.x0 + x0;
    subslice.area.x1 = slice->area.x0 + x1;
    subslice.area.y0 = slice->area.y0 + y0;
    subslice.area.y1 = slice->area.y0 + y1;
    return(subslice);
}

bool _make_glyph(Font *font, s32 codepoint, Glyph *glyph_out)
{
    stack_enter_frame();
    bool ok = _font_backend_make_glyph(&font->backend, codepoint, glyph_out);
    if (ok) {
        s32 data_size = glyph_out->width*glyph_out->height;
        if (!font->pixel_data || font->pixel_data->length + data_size > alen(font->pixel_data->data)) {
            Font::DataBlock *new_block = null;
            if (font->pixel_data_free) {
                new_block = font->pixel_data_free;
                font->pixel_data_free = new_block->previous;
            } else {
                new_block = (Font::DataBlock *) heap_alloc(sizeof(Font::DataBlock));
            }
            new_block->previous = font->pixel_data;
            font->pixel_data = new_block;
            new_block->length = 0;
        }
        u32 *new_data = font->pixel_data->data + font->pixel_data->length;
        memcpy(font->pixel_data->data + font->pixel_data->length, glyph_out->data, data_size*sizeof(u32));
        glyph_out->data = new_data;
        font->pixel_data->length += data_size;
        assert(((u64) new_data) % 16 == 0); // must be aligned for simd
    }
    stack_leave_frame();
    return(ok);
}

bool _get_glyph(Font *font, s32 codepoint, Glyph *glyph_out)
{
    if (codepoint <= 128) {
        bool available = font->ascii_glyph_map[codepoint >> 5] & (1u << (codepoint&31));
        if (available && glyph_out) *glyph_out = font->ascii_glyphs[codepoint];
        return(available);
    } else {
        u32 index = U32_MAX;
        if (!font->known_glyphs.get(codepoint, &index)) {
            Glyph new_glyph = {0};
            stack_enter_frame();
            if (_make_glyph(font, codepoint, &new_glyph)) {
                index = font->available.length;
                font->available.append(new_glyph);
            }
            stack_leave_frame();
            font->known_glyphs.insert(codepoint, index);
        }

        if (index != U32_MAX && glyph_out) *glyph_out = font->available[index];
        return(index != U32_MAX);
    }
}

void font_change_size(Font *font, s32 new_size)
{
    _font_backend_change(&font->backend, new_size, &font->metrics);

    font->available.clear();
    font->known_glyphs.clear();
    while (font->pixel_data) {
        Font::DataBlock *temp = font->pixel_data->previous;
        font->pixel_data->previous = font->pixel_data_free;
        font->pixel_data_free = font->pixel_data;
        font->pixel_data = temp;
    }
    memset(font->ascii_glyph_map, 0, sizeof(font->ascii_glyph_map));

    for (s32 codepoint = 32; codepoint < 128; ++codepoint) {
        Glyph glyph = {};
        bool available = _make_glyph(font, codepoint, &glyph);
        if (available) {
            font->ascii_glyph_map[codepoint >> 5] |= 1u << (codepoint & 31);
            font->ascii_glyphs[codepoint] = glyph;
        }

        // We need these codepoints to render escape sequences for unavailable codepoints
        if ((codepoint >= '0' && codepoint <= '9') || (codepoint >= 'a' && codepoint <= 'f') || (codepoint == '\\')) assert(available);
    }
}

void glyphs_for_codepoint(Font *font, DecodedCodepoint decoded, Glyph glyphs[8], s32 *glyph_count, bool *glyphs_are_escape_sequence)
{
    char escape_prefix = 0;
    if (decoded.valid) {
        Glyph glyph;
        if (_get_glyph(font, decoded.codepoint, &glyph)) {
            *glyph_count = 1;
            if (glyphs) glyphs[0] = glyph;
        } else {
            escape_prefix = 'u';
        }
    } else {
        escape_prefix = 'x';
    }

    if (escape_prefix) {
        s32 sequence_length = 0;
        char sequence[9];

        if (decoded.codepoint == '\n') {
            sequence_length = 2;
            sequence[0] = '\\';
            sequence[1] = 'n';
        } else if (decoded.codepoint == '\r') {
            sequence_length = 2;
            sequence[0] = '\\';
            sequence[1] = 'r';
        } else if (decoded.codepoint == '\t') {
            sequence_length = 2;
            sequence[0] = '\\';
            sequence[1] = 't';
        } else {
            sequence_length = stbsp_snprintf(sequence, array_length(sequence), "\\%c%02x", escape_prefix, decoded.codepoint);
            assert(sequence_length < array_length(sequence));
        }

        *glyph_count = sequence_length;
        if (glyphs) {
            for (s32 i = 0; i < sequence_length; ++i) {
                bool available = _get_glyph(font, sequence[i], &glyphs[i]);
                debug_assert(available);
            }
        }
    }

    if (glyphs_are_escape_sequence) *glyphs_are_escape_sequence = (escape_prefix != 0);
}

// NB (Morten, 2020-05-30) Needs to be in sync with the above function. Having this split out is better for the optimizer though.
s32 glyph_count_for_codepoint(Font *font, DecodedCodepoint decoded)
{
    if (decoded.codepoint <= 128 && font->ascii_glyph_map[decoded.codepoint >> 5] & (1u << (decoded.codepoint&31))) {
        return(1);
    } else {
        s32 count = 0;
        glyphs_for_codepoint(font, decoded, null, &count, null);
        return(count);
    }
}

s32 draw_text_ellipsis(DrawTargetSlice *slice, Font *font, str text, s32 x0, s32 x1, s32 y, u32 color, bool align_right)
{
    s32 max_glyphs = (x1 - x0) / font->metrics.advance;
    s32 count = 0;
    if (max_glyphs > 3) {
        stack_enter_frame();
        Glyph *glyphs = stack_alloc(Glyph, max_glyphs + 8);

        while (count < max_glyphs && text.length > 0) {
            DecodedCodepoint decoded = decode_utf8((u8 *) text.data, text.length);
            text.length -= decoded.length;
            text.data += decoded.length;

            s32 extra_count = 0;
            glyphs_for_codepoint(font, decoded, &glyphs[count], &extra_count, null);
            count += extra_count;
        }

        if (count >= max_glyphs) {
            count = max_glyphs;
            assert(_get_glyph(font, '.', &glyphs[max_glyphs - 3]));
            glyphs[max_glyphs - 2] = glyphs[max_glyphs - 3];
            glyphs[max_glyphs - 1] = glyphs[max_glyphs - 3];
        }

        s32 x = align_right? (x1 - count*font->metrics.advance) : x0;
        for (s32 i = 0; i < count; ++i) {
            draw_glyph(slice, font, glyphs[i], x, y, color);
            x += font->metrics.advance;
        }

        stack_leave_frame();
    }
    return(count * font->metrics.advance);
}

s32 draw_glyphs(DrawTargetSlice *slice, Font *font, DecodedCodepoint decoded, s32 x, s32 y, u32 color, u32 escape_color)
{
    if (escape_color == 0) escape_color = color;

    s32 count;
    Glyph glyphs[8];
    bool are_escape_sequence;
    glyphs_for_codepoint(font, decoded, glyphs, &count, &are_escape_sequence);

    for (s32 i = 0; i < count; ++i) {
        draw_glyph(slice, font, glyphs[i], x, y, are_escape_sequence? escape_color : color);
        x += font->metrics.advance;
    }

    return(font->metrics.advance * count);
}

void draw_glyph(DrawTargetSlice *slice, Font *font, Glyph glyph, s32 x, s32 y, u32 color)
{
    x += glyph.offset_x;
    y += glyph.offset_y;

    u32 *data = glyph.data;
    s32 data_stride = glyph.width;

    s32 x0 = x;
    s32 x1 = x + glyph.width;
    s32 y0 = y;
    s32 y1 = y + glyph.height;

    s32 slice_height = slice->area.y1 - slice->area.y0;

    if (y0 < slice_height && y1 >= 0) {

        // NB (Morten, 2020-07-11) Due to the way we implemented our simd blending, we can't really do clamping on the x-axis without extra effort. The way we currently render text from the app we only ever end up drawing text out-of-bounds on the y-axis though.
        if (y0 < 0) {
            data += (-y0) * data_stride;
            y0 = 0;
        }
        if (y1 > slice_height) {
            y1 = slice_height;
        }

        //debug_assert(0 <= x0 && x1 <= (slice->area.x1 - slice->area.x0));

        DrawCommand command = {0};
        command.x0 = slice->area.x0 + x0;
        command.x1 = slice->area.x0 + x1;
        command.y0 = slice->area.y0 + y0;
        command.y1 = slice->area.y0 + y1;
        command.kind = DrawCommand::GLYPH;
        command.color = color;
        command.glyph_data = data;
        _draw_target_add_command(slice->target, command);
    }
}

void draw_single_pixel(DrawTargetSlice *slice, s32 x, s32 y, u32 color)
{
    draw_solid(slice, x, y, x + 1, y + 1, color);
}

void draw_solid(DrawTargetSlice *slice, s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    s32 width = slice->area.x1 - slice->area.x0;
    s32 height = slice->area.y1 - slice->area.y0;

    DrawCommand command = {0};
    command.x0 = slice->area.x0 + clamp(x0, 0, width);
    command.x1 = slice->area.x0 + clamp(x1, 0, width);
    command.y0 = slice->area.y0 + clamp(y0, 0, height);
    command.y1 = slice->area.y0 + clamp(y1, 0, height);
    command.kind = DrawCommand::SOLID;
    command.color = color;
    _draw_target_add_command(slice->target, command);
}

void draw_blend(DrawTargetSlice *slice, s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    s32 width = slice->area.x1 - slice->area.x0;
    s32 height = slice->area.y1 - slice->area.y0;

    DrawCommand command = {0};
    command.x0 = slice->area.x0 + clamp(x0, 0, width);
    command.x1 = slice->area.x0 + clamp(x1, 0, width);
    command.y0 = slice->area.y0 + clamp(y0, 0, height);
    command.y1 = slice->area.y0 + clamp(y1, 0, height);
    command.kind = DrawCommand::MULTIPLY;
    command.color = color;
    _draw_target_add_command(slice->target, command);
}