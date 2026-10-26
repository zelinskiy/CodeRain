#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// Build:
// cl /std:c17 /W4 /TC /utf-8 main.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib

// #define DEBUG_CONSOLE
// #define FULLSCREEN

#define MAX_STREAM_SPEED 1000
#define MAX_STREAM_COUNT 200
#define MAX_STREAM_ROWS 2048
#define FALLBACK_WHITESPACE_WIDTH 12
#define HORIZONTAL_STREAM_PADDING 10

enum Language {
    EN,
    LA,
    GR,
    JP,
    CSL,
    AKK,
};

typedef struct {
    enum Language lang;
    wchar_t *file_name;
    wchar_t *font_familly;
    wchar_t *matrix_chars;    
    int font_size;
    int stream_count;
    int stream_speed;
    int vertical_spacing;
    int whitespace_length;
    int horizontal_stream_padding;
    int color_r;
    int color_g;
    int color_b;
} Settings;

typedef struct {
    wchar_t value;
    wchar_t diacritic1;
    wchar_t diacritic2;
    int width;
    int height;
    int base_width;
    int base_offset_y;
    int dia1_offset_y;
    int dia2_offset_y;
} StreamCharacter;

typedef struct {
    int active;
    int spawn_delay;
    int x;
    int start_y;
    int head_row;        // Current head character index
    int first_row;       // Current tail character index
    int char_count;      // Total characters in this chunk
    int max_width;       // Max width among characters in this chunk
    int max_height;      // Total height of all characters in this chunk
    StreamCharacter chars[MAX_STREAM_ROWS];
} Stream;

static const wchar_t CLASS_NAME[] = L"MatrixScreensaverWindow";

static const wchar_t EN_MATRIX_CHARS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const wchar_t LA_MATRIX_CHARS[] = L"ABCDEFGHILMNOPQRSTVX";
static const wchar_t MATRIX_CHARS[] = L"ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ";
static const wchar_t CSL_MATRIX_CHARS[] = L"АБВГДЕЖЅЗИІКЛМНОПРСТѸФХѠЦЧШЩЪЫЬѢЮꙖѤѦѨѪѬѮѰѲѴѶ";
static const wchar_t JP_MATRIX_CHARS[] =
    L"アイウエオカキクケコサシスセソ"
    L"タチツテトナニヌネノハヒフヘホ"
    L"マミムメモヤユヨラリルレロワン";
static const wchar_t AKK_MATRIX_CHARS[] =
    L"\U0001202D\U00012000\U00012157\U000121A4\U00012153\U00012079\U0001230C\U00012097\U0001222C\U00012200"
    L"\U00012250\U00012040\U0001227A\U00012248\U000122E2\U0001236A\U0001223E\U00012223\U00012261\U000121B7"
    L"\U00012038\U00012337\U000120E2\U00012054\U000120F6\U00012108\U0001212C\U00012034\U000122DB\U000122DE"
    L"\U000122B9\U0001213F\U00012140\U00012317\U000122A0\U0001224C\U00012258\U000122C3\U000122FD\U000122FB"
    L"\U0001231D\U00012328\U0001232A\U0001235C\U00012005\U00012009\U0001200B\U0001201D\U0001202A\U0001202E"
    L"\U00012039\U00012044\U00012047\U00012048\U00012055\U0001205E\U00012068\U0001207A\U0001208D\U0001209B"
    L"\U000120C0\U000120E4\U00012111\U0001212D\U00012134\U00012137\U000121A0\U000121B4\U000121C4\U0001221B"
    L"\U00012239\U00012240\U0001227E\U0001228C\U00012295\U000122B7\U000122CF\U000122D4\U000122E9\U0001230B"
    L"\U00012311\U0001232B\U0001233D\U00012347\U0001235F\U0001236E\U0001200F\U000120A0\U000120B7\U000120FB"
    L"\U00012139\U00012174\U000121F7\U00012228\U0001225C\U00012293\U00012325\U0001233B\U0001235A\U0001236D";


static const wchar_t DIACRITICS[] = { 
    0x0300,  // COMBINING GRAVE ACCENT
    0x0301,  // COMBINING ACUTE ACCENT
    0x0311,  // COMBINING INVERTED BREVE
    0x033E,  // COMBINING VERTICAL TILDE
    0x0483,  // COMBINING CYRILLIC TITLO
    0x0486,  // COMBINING CYRILLIC PSILI PNEUMATA
    0x2DE3,  // COMBINING CYRILLIC LETTER DE
    0x2DED,  // COMBINING CYRILLIC LETTER ES
    0x0487,  // COMBINING CYRILLIC POKRYTIE
}; 
static const wchar_t OMIT_SYMBOLS[] = {
    0x003A,     // Colon (:)
    0x002C,     // Comma (,)
    0x002E,     // Full stop (.)
    0x003B,     // Semicolon (;)
    0x0021,     // Exclamation mark (!)
    0x003F,     // Question mark (?)
    0x2014,     // Em dash (—)
    0x002D,     // Hyphen-minus (-)
    0x0028,     // Left parenthesis (()
    0x0029,     // Right parenthesis ())
    0x00AB,     // Left-pointing double angle quotation mark («)
    0x00BB,     // Right-pointing double angle quotation mark (»)
    0x201C,     // Left double quotation mark (“)
    0x201D,     // Right double quotation mark (”)
    0x2018,     // Left single quotation mark (‘)
    0x2019,     // Right single quotation mark (’)
    0xA67E,     // CYRILLIC KAVYKA (꙾) -- these actually shall be cleaned up
    0x005B,     // Left square bracket ([)
    0x005D,     // Right square bracket (])
    0x2424,     // Newline symbol
    L'\0'
};

static Settings settings = {
    .lang = CSL,
    .file_name = L"psalter.txt",
    .font_familly = L"Ponomar",
    .matrix_chars =  L"АБВГДЕЖЅЗИІКЛМНОПРСТѸФХѠЦЧШЩЪЫЬѢЮꙖѤѦѨѪѬѮѰѲѴѶ",
    .font_size = 30,
    .stream_count = 20,
    .stream_speed = 700,
    .vertical_spacing = 3,
    .whitespace_length = 15,
    .horizontal_stream_padding = 10,
    .color_r = 255,
    .color_g = 0,
    .color_b = 0,
};

static Stream streams[MAX_STREAM_COUNT];
static int streams_initialized = 0;

static HFONT stream_font = NULL;
static FILE *text_source_file = NULL;

// Backbuffer
static HDC backbuffer_dc = NULL;
static HBITMAP backbuffer_bitmap = NULL;
static HBITMAP backbuffer_old_bitmap = NULL;
static int backbuffer_width = 0;
static int backbuffer_height = 0;

static GLYPHMETRICS GetCharacterDimensions(HDC hdc, wchar_t ch)
{
    GLYPHMETRICS gm = { 0 };
    if (ch == L'\0' || ch == L' ') {
        return gm;
    }
    HGDIOBJ hOldFont = SelectObject(hdc, stream_font);
    MAT2 mat2 = { 
        {0, 1}, {0, 0},
        {0, 0}, {0, 1},
    };
    GetGlyphOutlineW(hdc, (UINT)ch, GGO_METRICS, &gm, 0, NULL, &mat2);
    SelectObject(hdc, hOldFont);
    return gm;
}

static int GetCharacterWidth(HDC hdc, wchar_t ch)
{
    int ch_width = 0;
    HGDIOBJ hOldFont = SelectObject(hdc, stream_font);
    GetCharWidth32W(hdc, ch, ch, &ch_width);
    SelectObject(hdc, hOldFont);
    return ch_width;
}

static void AdHocCharFixes(StreamCharacter *sc){
    if (sc->value == L'ц') {
        sc->height -= 10;
    } else if (sc->value == L'р') {
        sc->height -= 5;
    }

    // с/д titlovannoe
    if (sc->diacritic1 == 0x2DE3 || sc->diacritic1 == 0x2DED) {
        sc->dia1_offset_y += 3;
        sc->dia2_offset_y += 3;
        sc->base_offset_y += 4;
        sc->height += 3;
    }
}

static void MeasureStreamCharacter(HDC hdc, StreamCharacter *sc)
{
    if (sc->value == L' ') {
        sc->width = GetCharacterWidth(hdc, L' ');
        if (sc->width < 1) {
            sc->width = FALLBACK_WHITESPACE_WIDTH;
        }
        sc->height = settings.whitespace_length;
        sc->base_width = sc->width;
        sc->base_offset_y = 0;
        sc->dia1_offset_y = 0;
        sc->dia2_offset_y = 0;
        return;
    }

    GLYPHMETRICS ch_gm = GetCharacterDimensions(hdc, sc->value);
    int ch_w = GetCharacterWidth(hdc, sc->value);
    int w = max(ch_w, (int)ch_gm.gmBlackBoxX);
    int dia_h = 0;
    int dia1_y = 0;
    int dia2_y = 0;

    if(sc->diacritic1 != L'\0') {
        GLYPHMETRICS d1 = GetCharacterDimensions(hdc, sc->diacritic1);
        dia_h = (int)d1.gmBlackBoxY;
        w = max(w, ch_w + (int)d1.gmBlackBoxX);
        dia1_y = ch_gm.gmptGlyphOrigin.y + (int)d1.gmBlackBoxY;
    }
    
    if (sc->diacritic2 != L'\0') {
        GLYPHMETRICS d2 = GetCharacterDimensions(hdc, sc->diacritic2);
        dia_h = max(dia_h, (int)d2.gmBlackBoxY);
        w = max(w, ch_w + (int)d2.gmBlackBoxX);
        dia2_y = ch_gm.gmptGlyphOrigin.y + (int)d2.gmBlackBoxY;
    }

    int height = max((int)ch_gm.gmBlackBoxY + dia_h + settings.vertical_spacing, 1);

    sc->width = max(w, 1);
    sc->height = height;
    sc->base_width = ch_w;
    sc->base_offset_y = dia_h + ch_gm.gmptGlyphOrigin.y;
    sc->dia1_offset_y = dia1_y;
    sc->dia2_offset_y = dia2_y;

    AdHocCharFixes(sc);
}

static FILE *OpenTextFile(const wchar_t *filename)
{
    FILE *fp = NULL;
    if (_wfopen_s(&fp, filename, L"rb") == 0 && fp != NULL) {
        return fp;
    }

    return NULL;
}

static BOOL InitTextSource(const wchar_t *filename)
{
    text_source_file = OpenTextFile(filename);
    return text_source_file != NULL;
}

static void CleanupTextSource(void)
{
    if (text_source_file != NULL) {
        fclose(text_source_file);
        text_source_file = NULL;
    }
}

static BOOL GetNextChar(FILE *fp, wchar_t *out_char)
{
    if (fp == NULL || out_char == NULL) {
        return FALSE;
    }

    int b1 = fgetc(fp);
    if (b1 == EOF) {
        rewind(fp);
        b1 = fgetc(fp);
        // Prevent from dumb looping in an empty file
        if (b1 == EOF) {
            return FALSE;
        }
    }

    wchar_t ch;
    if ((b1 & 0x80) == 0) {
        // 1-byte ASCII
        ch = (wchar_t)b1;
    } else if ((b1 & 0xE0) == 0xC0) {
        // 2-byte UTF-8 sequence
        int b2 = fgetc(fp);
        if (b2 == EOF) { rewind(fp); b2 = fgetc(fp); }
        ch = (wchar_t)(((b1 & 0x1F) << 6) | (b2 & 0x3F));
    } else if ((b1 & 0xF0) == 0xE0) {
        // 3-byte UTF-8 sequence
        int b2 = fgetc(fp);
        if (b2 == EOF) { rewind(fp); b2 = fgetc(fp); }
        int b3 = fgetc(fp);
        if (b3 == EOF) { rewind(fp); b3 = fgetc(fp); }
        ch = (wchar_t)(((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));
    } else {
        ch = (wchar_t)b1;
    }

    // Skip UTF-8 BOM
    if (ch == 0xFEFF) {
        return GetNextChar(fp, out_char);
    }

    *out_char = ch;
    return TRUE;
}

static BOOL IsDiacritic(wchar_t ch)
{
    for (size_t i = 0; DIACRITICS[i] != L'\0'; i++) {
        if (DIACRITICS[i] == ch) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL IsOmitSymbol(wchar_t ch)
{
    for (size_t i = 0; OMIT_SYMBOLS[i] != L'\0'; i++) {
        if (OMIT_SYMBOLS[i] == ch) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL IsChunkDelimiter(wchar_t ch)
{
    return (ch == L';' || ch == L':' || ch == L'.');
}

static size_t ReadTextChunk(StreamCharacter *dest, size_t max_count)
{
    if (text_source_file == NULL || max_count == 0) {
        return 0;
    }

    wchar_t ch;

    // Skip leading whitespace, newlines, and delimiters from the previous chunk
    for (;;) {
        if (!GetNextChar(text_source_file, &ch)) {
            return 0;
        }
        if (ch != L' ' && ch != L'\r' && ch != L'\n' && !IsChunkDelimiter(ch)) {
            break;
        }
    }

    size_t total_read = 0;
    if (!IsOmitSymbol(ch) && !IsDiacritic(ch)) {
        dest[total_read].value = ch;
        dest[total_read].diacritic1 = L'\0';
        dest[total_read].diacritic2 = L'\0';
        total_read++;
    }

    while (total_read < max_count) {
        if (!GetNextChar(text_source_file, &ch)) {
            break;
        }

        if (IsChunkDelimiter(ch)) {
            break;
        }

        if (IsOmitSymbol(ch)) {
            continue;
        }

        if (!IsDiacritic(ch)) {
            dest[total_read].value = ch;
            dest[total_read].diacritic1 = L'\0';
            dest[total_read].diacritic2 = L'\0';
            total_read++;
        } else if (total_read > 0) {
            if (dest[total_read - 1].diacritic1 == L'\0') {
                dest[total_read - 1].diacritic1 = ch;
            } else if (dest[total_read - 1].diacritic2 == L'\0') {
                dest[total_read - 1].diacritic2 = ch;
            }
        }
    }

    // Trim trailing whitespace if any
    while (total_read > 0 && dest[total_read - 1].value == L' ') {
        total_read--;
    }

    return total_read;
}

static void CleanupBackbuffer(void)
{
    if (backbuffer_dc != NULL) {
        if (backbuffer_old_bitmap != NULL) {
            SelectObject(backbuffer_dc, backbuffer_old_bitmap);
            backbuffer_old_bitmap = NULL;
        }

        if (backbuffer_bitmap != NULL) {
            DeleteObject(backbuffer_bitmap);
            backbuffer_bitmap = NULL;
        }

        DeleteDC(backbuffer_dc);
        backbuffer_dc = NULL;
    }

    backbuffer_width = 0;
    backbuffer_height = 0;
}

static BOOL EnsureBackbuffer(HDC reference_dc, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return FALSE;
    }

    if (backbuffer_dc != NULL &&
        backbuffer_bitmap != NULL &&
        backbuffer_width == width &&
        backbuffer_height == height) {
        return TRUE;
    }

    CleanupBackbuffer();

    HDC new_dc = CreateCompatibleDC(reference_dc);
    if (new_dc == NULL) {
        return FALSE;
    }

    HBITMAP new_bitmap =
        CreateCompatibleBitmap(reference_dc, width, height);

    if (new_bitmap == NULL) {
        DeleteDC(new_dc);
        return FALSE;
    }

    HBITMAP old_bitmap =
        (HBITMAP)SelectObject(new_dc, new_bitmap);

    if (old_bitmap == NULL || old_bitmap == (HBITMAP)HGDI_ERROR) {
        DeleteObject(new_bitmap);
        DeleteDC(new_dc);
        return FALSE;
    }

    backbuffer_dc = new_dc;
    backbuffer_bitmap = new_bitmap;
    backbuffer_old_bitmap = old_bitmap;
    backbuffer_width = width;
    backbuffer_height = height;

    return TRUE;
}

static BOOL InitFontMetrics(HWND hwnd)
{
    if (stream_font == NULL) {
        stream_font = CreateFontW(
            settings.font_size * (-1),
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN,
            settings.font_familly
        );

        if (stream_font == NULL) {
            return FALSE;
        }
    }

    HDC hdc = GetDC(hwnd);
    if (hdc == NULL) {
        return FALSE;
    }

    HFONT old_font = (HFONT)SelectObject(hdc, stream_font);
    TEXTMETRICW tm;
    BOOL metrics_ok = GetTextMetricsW(hdc, &tm);
    if (old_font != NULL) {
        SelectObject(hdc, old_font);
    }
    ReleaseDC(hwnd, hdc);

    return metrics_ok;
}

static void CleanupFont(void)
{
    if (stream_font != NULL) {
        DeleteObject(stream_font);
        stream_font = NULL;
    }
}

static RECT GetStreamRect(const Stream *stream)
{
    RECT r = { 0, 0, 0, 0 };
    if (!stream->active) {
        return r;
    }

    r.left = stream->x;
    r.right = stream->x + stream->max_width;

    int first_row = stream->first_row;
    int last_row = stream->head_row;
    if (last_row >= stream->char_count) {
        last_row = stream->char_count - 1;
    }

    if (first_row > last_row) {
        r.top = stream->start_y;
        r.bottom = stream->start_y;
        return r;
    }

    int y = stream->start_y;
    for (int i = 0; i < first_row && i < stream->char_count; ++i) {
        y += stream->chars[i].height;
    }
    r.top = y;

    for (int i = first_row; i <= last_row && i < stream->char_count; ++i) {
        y += stream->chars[i].height;
    }
    r.bottom = y;

    return r;
}

static BOOL CheckStreamCollision(const Stream *stream, int cand_x, int cand_width)
{
    int cand_left = cand_x;
    int cand_right = cand_x + cand_width;

    for (int i = 0; i < settings.stream_count; ++i) {
        const Stream *other = &streams[i];
        if (other == stream || !other->active) {
            continue;
        }

        // Horizontal lane overlap check with padding
        if (cand_right + HORIZONTAL_STREAM_PADDING > other->x &&
            other->x + other->max_width + HORIZONTAL_STREAM_PADDING > cand_left) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL SpawnStream(HDC hdc, Stream *stream, int screen_width, int screen_height)
{
    if (screen_width <= 0 || screen_height <= 0) {
        return FALSE;
    }

    memset(stream->chars, 0, sizeof(stream->chars));

    size_t grabbed = 0;
    if (text_source_file != NULL) {
        grabbed = ReadTextChunk(stream->chars, MAX_STREAM_ROWS);
    }

    if (grabbed == 0) {
        const int char_count = (int)(sizeof(MATRIX_CHARS) / sizeof(MATRIX_CHARS[0])) - 1;
        grabbed = 25 + rand() % 25;
        for (size_t k = 0; k < grabbed; ++k) {
            stream->chars[k].value = MATRIX_CHARS[rand() % char_count];
            stream->chars[k].diacritic1 = L'\0';
            stream->chars[k].diacritic2 = L'\0';
        }
    }

    int accum_h = 0;
    int max_w = 0;

    for (size_t i = 0; i < grabbed; ++i) {
        MeasureStreamCharacter(hdc, &stream->chars[i]);
        accum_h += stream->chars[i].height;
        if (stream->chars[i].width > max_w) {
            max_w = stream->chars[i].width;
        }
    }

    stream->char_count = (int)grabbed;
    stream->max_width = max(max_w, 1);
    stream->max_height = accum_h;

    int max_x = screen_width - stream->max_width;
    if (max_x < 1) {
        max_x = 1;
    }

    int max_y = screen_height - 60;
    if (max_y < 1) {
        max_y = 1;
    }

    // Try several random positions across the screen before giving up
    for (int attempt = 0; attempt < 200; ++attempt) {
        const int cand_x = rand() % max_x;
        const int cand_y = rand() % max_y;

        if (CheckStreamCollision(stream, cand_x, stream->max_width)) {
            continue;
        }

        stream->x = cand_x;
        stream->start_y = cand_y;
        stream->head_row = 0;
        stream->first_row = 0;
        stream->active = 1;
        stream->spawn_delay = 0;

        return TRUE;
    }

    return FALSE;
}

static void InitStreams(HDC hdc, int screen_width, int screen_height)
{
    int initial_count = settings.stream_count;

    for (int i = 0; i < initial_count; ++i) {
        if (!SpawnStream(hdc, &streams[i], screen_width, screen_height)) {
            streams[i].active = 0;
            streams[i].spawn_delay = 1 + rand() % 20;
        }
    }

    for (int i = initial_count; i < settings.stream_count; ++i) {
        streams[i].active = 0;
        streams[i].spawn_delay = 10 + rand() % 41;
    }

    streams_initialized = 1;
}

static void UpdateStreams(HDC hdc, int screen_width, int screen_height)
{
    if (screen_width < 1 || screen_height < 1) {
        return;
    }

    if (!streams_initialized) {
        InitStreams(hdc, screen_width, screen_height);
    }

    for (int i = 0; i < settings.stream_count; ++i) {
        Stream *stream = &streams[i];

        if (!stream->active) {
            if (stream->spawn_delay > 0) {
                --stream->spawn_delay;
            }

            if (stream->spawn_delay == 0) {
                if (!SpawnStream(hdc, stream, screen_width, screen_height)) {
                    stream->spawn_delay = 5;
                }
            }

            continue;
        }

        // Calculate Y position of the bottom of the current head character
        int head_bottom_y = stream->start_y;
        for (int r = 0; r <= stream->head_row && r < stream->char_count; ++r) {
            head_bottom_y += stream->chars[r].height;
        }

        // Check if head can still advance down
        BOOL head_can_advance = (head_bottom_y < screen_height) && 
                                (stream->head_row + 1 < stream->char_count);

        if (head_can_advance) {
            ++stream->head_row;
        } else {
            ++stream->first_row;
        }

        // Calculate Y position of the top of the tail character (first_row)
        int tail_top_y = stream->start_y;
        for (int r = 0; r < stream->first_row && r < stream->char_count; ++r) {
            tail_top_y += stream->chars[r].height;
        }

        // If tail has run off the bottom of the screen OR passed all characters:
        if (stream->first_row > stream->head_row || 
            stream->first_row >= stream->char_count || 
            tail_top_y >= screen_height) {
            stream->active = 0;
            stream->spawn_delay = 1 + rand() % 20;
        }
    }
}

static void InvalidateStream(HWND hwnd, const Stream *stream)
{
    if (!stream->active) {
        return;
    }

    RECT rect = GetStreamRect(stream);
    InvalidateRect(hwnd, &rect, FALSE);
}

static void RenderFrame(HWND hwnd, HDC hdc, const RECT *paint_rect)
{
    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    const int width = client_rect.right;
    const int height = client_rect.bottom;

    if (!EnsureBackbuffer(hdc, width, height)) {
        return;
    }

    HDC draw_dc = backbuffer_dc;
    const int saved_dc = SaveDC(draw_dc);

    if (saved_dc == 0) {
        return;
    }

    IntersectClipRect(
        draw_dc,
        paint_rect->left,
        paint_rect->top,
        paint_rect->right,
        paint_rect->bottom
    );

    FillRect(
        draw_dc,
        paint_rect,
        (HBRUSH)GetStockObject(BLACK_BRUSH)
    );

    HFONT old_font = NULL;
    if (stream_font != NULL) {
        old_font = (HFONT)SelectObject(draw_dc, stream_font);
    }

    SetBkMode(draw_dc, TRANSPARENT);

    for (int i = 0; i < settings.stream_count; ++i) {
        const Stream *stream = &streams[i];

        if (!stream->active) {
            continue;
        }

        if (stream->x >= paint_rect->right ||
            stream->x + stream->max_width <= paint_rect->left) {
            continue;
        }

        int first_row = stream->first_row;
        int last_row = stream->head_row;
        if (last_row >= stream->char_count) {
            last_row = stream->char_count - 1;
        }

        if (first_row > last_row) {
            continue;
        }

        int offset_y = stream->start_y;
        for (int r = 0; r < first_row && r < stream->char_count; ++r) {
            offset_y += stream->chars[r].height;
        }

        int count_visible = last_row - first_row + 1;
        if (count_visible < 1) count_visible = 1;

        for (int row = first_row; row <= last_row; ++row) {
            if (offset_y >= paint_rect->bottom) {
                break;
            }

            const int distance = stream->head_row - row;
            int red = 255 - distance * 200 / count_visible;
            if (red < 40) red = 40;
            if (red > 255) red = 255;

            SetTextColor(draw_dc, RGB(red, 0, 0));

            const StreamCharacter *sc = &stream->chars[row];

            if (sc->value == L' ') {
                offset_y += sc->height;
                continue;
            }

            int ch_offset_x = (stream->max_width - sc->base_width) / 2;

            if (sc->diacritic1 != L'\0') {
                TextOutW(
                    draw_dc,
                    stream->x + ch_offset_x + sc->base_width,
                    offset_y + sc->dia1_offset_y,
                    &sc->diacritic1,
                    1
                );
            }

            if (sc->diacritic2 != L'\0') {
                TextOutW(
                    draw_dc,
                    stream->x + ch_offset_x + sc->base_width,
                    offset_y + sc->dia2_offset_y,
                    &sc->diacritic2,
                    1
                );
            }

            TextOutW(
                draw_dc,
                stream->x + ch_offset_x,
                offset_y + sc->base_offset_y,
                &sc->value,
                1
            );

            offset_y += sc->height;
        }
    }

    if (old_font != NULL) {
        SelectObject(draw_dc, old_font);
    }

    RestoreDC(draw_dc, saved_dc);

    const int paint_width = paint_rect->right - paint_rect->left;
    const int paint_height = paint_rect->bottom - paint_rect->top;

    if (paint_width > 0 && paint_height > 0) {
        BitBlt(
            hdc,
            paint_rect->left,
            paint_rect->top,
            paint_width,
            paint_height,
            backbuffer_dc,
            paint_rect->left,
            paint_rect->top,
            SRCCOPY
        );
    }
}

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            InitTextSource(settings.file_name);

            if (!InitFontMetrics(hwnd)) {
                CleanupTextSource();
                return -1;
            }

            int stream_speed = MAX_STREAM_SPEED - (settings.stream_speed % MAX_STREAM_SPEED);

            if (SetTimer(hwnd, 1, stream_speed, NULL) == 0) {
                CleanupFont();
                CleanupTextSource();
                return -1;
            }
            return 0; 
        }       
        case WM_TIMER:
        {
            RECT client_rect;
            GetClientRect(hwnd, &client_rect);
            HDC hdc = GetDC(hwnd);
            UpdateStreams(hdc, client_rect.right, client_rect.bottom);
            ReleaseDC(hwnd, hdc);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RenderFrame(hwnd, hdc, &ps.rcPaint);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
        {
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_DESTROY:
        {
            KillTimer(hwnd, 1);
            CleanupBackbuffer();
            CleanupFont();
            CleanupTextSource();
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (RegisterClassW(&wc) == 0)
    {
        return 1;
    }

    #ifdef FULLSCREEN
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Matrix Screensaver",
        WS_POPUP | WS_VISIBLE,
        0, 
        0,
        screenWidth,
        screenHeight,
        NULL, 
        NULL, 
        hInstance, 
        NULL
    );
    #else
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Matrix Screensaver",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    #endif

    if (hwnd == NULL)
    {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);

    #ifdef DEBUG_CONSOLE
    AllocConsole();
    FILE *console;
    freopen_s(&console, "CONOUT$", "w", stdout);
    #endif

    MSG msg;    

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}