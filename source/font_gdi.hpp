#pragma once
#include "win32.hpp"

struct FontBackend
{
    void *hfont;
    void *hbitmap;
    void *hdc;
    u32 *bitmap_data;

    enum { MAX_GLYPH_SIZE = 128 };
};

void _font_backend_change(FontBackend *backend, s32 new_size, FontMetrics *metrics)
{
    if (backend->hfont) win32::DeleteObject(backend->hfont);

    win32::logfont_w logfont = {};
    logfont.Height = -((new_size * 96) / 72);
    logfont.CharSet = win32::ANSI_CHARSET;
    logfont.Weight = win32::FW_REGULAR;
    logfont.Quality = win32::CLEARTYPE_QUALITY;
    wchar_t *font_name = L"Consolas";
    memcpy(logfont.FaceName, font_name, cstring_length(font_name)*sizeof(wchar_t));
    backend->hfont = win32::CreateFontIndirectW(&logfont);
    assert(backend->hfont);

    if (!backend->hdc) {
        backend->hdc = win32::CreateCompatibleDC(null);
        assert(backend->hdc);

        win32::bitmapinfo bmp = {};
        bmp.Header.Size = sizeof(win32::bitmapinfoheader);
        bmp.Header.Width = FontBackend::MAX_GLYPH_SIZE;
        bmp.Header.Height = FontBackend::MAX_GLYPH_SIZE;
        bmp.Header.Planes = 1;
        bmp.Header.BitCount = 32;
        bmp.Header.Compression = win32::BI_RGB;
        backend->hbitmap = win32::CreateDIBSection(backend->hdc, &bmp, win32::DIB_RGB_COLORS, (void **) &backend->bitmap_data, null, 0);
        assert(backend->hbitmap);
    }

    win32::SelectObject(backend->hdc, backend->hfont);
    win32::SelectObject(backend->hdc, backend->hbitmap);
    win32::SetTextColor(backend->hdc, 0xffffff);
    win32::SetBkColor(backend->hdc, 0x000000);

    win32::textmetric_w gdi_metrics = {};
    win32::GetTextMetricsW(backend->hdc, &gdi_metrics);

    metrics->size = new_size;
    metrics->ascent = gdi_metrics.Ascent;
    metrics->descent = -gdi_metrics.Descent;
    metrics->line_height = max(gdi_metrics.Height, gdi_metrics.Ascent + gdi_metrics.Descent + 1);
    metrics->advance = gdi_metrics.AveCharWidth;
}

bool _font_backend_make_glyph(FontBackend *backend, s32 codepoint, Glyph *glyph)
{
    bool supported = false;
    if (codepoint < 0xffff) {
        for (s32 i = 0; i < array_length(UNICODE_SUPPORTED_RANGES) && !supported; ++i) {
            supported = UNICODE_SUPPORTED_RANGES[i].min <= codepoint && codepoint <= UNICODE_SUPPORTED_RANGES[i].max;
        }
    }

    bool ok = false;
    if (supported) {
        wchar_t fake_utf16 = codepoint;

        u16 glyph_index = 0;
        win32::GetGlyphIndicesW(backend->hdc, &fake_utf16, 1, &glyph_index, win32::GGI_MARK_NONEXISTING_GLYPHS);
        bool missing = glyph_index == 0xffff;

        if (!missing) {
            win32::abc abc = {};
            win32::size size = {};
            win32::GetCharABCWidthsW(backend->hdc, codepoint, codepoint, &abc);
            win32::GetTextExtentPoint32W(backend->hdc, &fake_utf16, 1, &size);

            glyph->codepoint = codepoint;
            glyph->offset_x = abc.A;
            glyph->width = round_up(abc.B, 4);

            s32 draw_offset = max(0, -abc.A);
            s32 read_offset = max(0, abc.A);
            win32::ExtTextOutW(backend->hdc, draw_offset, 0, win32::ETO_GLYPH_INDEX, null, (wchar_t *) &glyph_index, 1, null);

            s32 y0 = 0;
            s32 y1 = size.Y;
            while (y0 < y1 && mem_is_zero(&backend->bitmap_data[read_offset + (FontBackend::MAX_GLYPH_SIZE - y0 - 1)*FontBackend::MAX_GLYPH_SIZE], abc.B*4)) ++y0;
            while (y0 < y1 && mem_is_zero(&backend->bitmap_data[read_offset + (FontBackend::MAX_GLYPH_SIZE - y1)*FontBackend::MAX_GLYPH_SIZE], abc.B*4)) --y1;

            if (y0 != y1 || codepoint == ' ') {
                glyph->offset_y = y0;
                glyph->height = y1 - y0;
    
                glyph->data = stack_alloc(u32, glyph->width*glyph->height);
                memset(glyph->data, 0, glyph->width*glyph->height*4);
    
                for (s32 y = 0; y < glyph->height; ++y) {
                    memcpy(glyph->data + y*glyph->width, &backend->bitmap_data[read_offset + (FontBackend::MAX_GLYPH_SIZE - (y0 + y) - 1)*FontBackend::MAX_GLYPH_SIZE], abc.B*4);
                }

                ok = true;
            }
        }
    }

    return(ok);
}