#define WIN32_LEAN_AND_MEAN
#define __STDC_WANT_LIB_EXT1__ 1

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <time.h>
#include <scrnsave.h>

// Build:
// cl /std:c17 /W4 /TC /utf-8 main.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
// cl /std:c17 /W4 /TC /utf-8 main.c /Fecoderain.scr /link /SUBSYSTEM:WINDOWS scrnsavw.lib user32.lib gdi32.lib comctl32.lib

// #define DEBUG_CONSOLE
#define FULLSCREEN

#define MAX_STREAM_SPEED 1000
#define MAX_STREAM_COUNT 200
#define MAX_MATRIX_CHARS 512
#define MAX_STREAM_ROWS 2048
#define FALLBACK_WHITESPACE_WIDTH 12
#define HORIZONTAL_STREAM_PADDING 10
#define N_DIACRITICS 8

enum Language {
    EN,
    RU,
    LA,
    GR,
    JP,
    CSL,
    AKK,
};

typedef struct {
    enum Language lang;
    wchar_t file_name[256];
    wchar_t font_familly[64];
    wchar_t matrix_chars[MAX_MATRIX_CHARS];
    int matrix_chars_count;
    int font_size;
    int stream_count;
    int stream_speed;
    int min_stream_length;
    int max_stream_length;
    int vertical_spacing;
    int whitespace_length;
    int horizontal_stream_padding;
    int color_r;
    int color_g;
    int color_b;
} Settings;

typedef struct {
    wchar_t value;
    wchar_t diacritics[N_DIACRITICS + 1];
    int width;
    int height;
    int base_width;
    int base_offset_y;
    int diacritic_padding_top;
    int diacritic_padding_bottom;
} StreamCharacter;

typedef struct {
    int active;
    int spawn_delay;
    int x;
    int start_y;
    int head_row; 
    int first_row;
    int char_count;
    int max_width;
    int max_height;
    StreamCharacter chars[MAX_STREAM_ROWS];
} Stream;

static const wchar_t CLASS_NAME[] = L"Code Rain";

static const wchar_t EN_MATRIX_CHARS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const wchar_t LA_MATRIX_CHARS[] = L"ABCDEFGHILMNOPQRSTVX";
static const wchar_t GR_MATRIX_CHARS[] = L"ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ";
static const wchar_t RU_MATRIX_CHARS[] = L"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
static const wchar_t CSL_MATRIX_CHARS[] = L"АБВГДЕЖЅЗИІКЛМНОПРСТѸФХѠЦЧШЩЪЫЬѢЮꙖѤѦѨѪѬѮѰѲѴѶ";
static const wchar_t JP_MATRIX_CHARS[] =
    L"アイウエオカキクケコサシスセソ"
    L"タチツテトナニヌネノハヒフヘホ"
    L"マミムメモヤユヨラリルレロワン";
static const wchar_t AKK_MATRIX_CHARS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";


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
    0x003A, 0x002C, 0x002E, 0x003B, 0x0021, 0x003F, 0x2014, 0x002D,
    0x0028, 0x0029, 0x00AB, 0x00BB, 0x201C, 0x201D, 0x2018, 0x2019,
    0xA67E, 0x005B, 0x005D, 0x2424, L'\0'
};

static Settings settings = {
    .lang = JP,
    .file_name = L'\0',
    .font_familly = L"MS Gothic",
    .matrix_chars = L'\0',
    .matrix_chars_count = 0,
    .font_size = 10,
    .stream_count = 20,
    .stream_speed = 700,
    .min_stream_length = 5,
    .max_stream_length = 25,
    .vertical_spacing = 3,
    .whitespace_length = 15,
    .horizontal_stream_padding = 10,
    .color_r = 0,
    .color_g = 255,
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


static void LogError(const char *msg) {
    if (!msg) return;

    time_t t;
    time(&t);
    struct tm tm;
    localtime_s(&tm, &t);
    char tstr[32];
    strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", &tm);

    FILE *fp;
    if (fopen_s(&fp, "err.log", "a") == 0 && fp) {
        size_t len = strlen(msg);
        while (len > 0 && (msg[len - 1] == '\n' || msg[len - 1] == '\r')) len--;
        fprintf(fp, "[%s] [ERROR] %.*s\n", tstr, (int)len, msg);
        fclose(fp);
    }
#ifdef DEBUG_CONSOLE
    printf("[%s] Alert: %s\n", tstr, msg);
#endif

}

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
    
    if(settings.lang == CSL) {
        if (sc->value == L'ц') {
            sc->height -= 10;
        } 
        
        if (sc->value == L'р') {
            sc->height -= 5;
        }   

        for(int i=0; i<N_DIACRITICS; ++i){
            wchar_t d = sc->diacritics[i];
            if(d == L'\0') break;

            // с/д titlovannoe
            if (d == 0x2DE3 || d == 0x2DED) {
                sc->diacritic_padding_top += 3;
                sc->base_offset_y += 4;
                sc->height += 3;
            }
        }
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
        sc->diacritic_padding_top = 0;
        return;
    }

    GLYPHMETRICS ch_gm = GetCharacterDimensions(hdc, sc->value);
    int ch_w = GetCharacterWidth(hdc, sc->value);
    int w = max(ch_w, (int)ch_gm.gmBlackBoxX);
    int dia_h = 0;
    int dia_y = 0;

    for(int i=0; i<N_DIACRITICS; ++i){
        wchar_t dia = sc->diacritics[i];
        if(dia != L'\0') {
            GLYPHMETRICS dia_gm = GetCharacterDimensions(hdc, dia);
            dia_h = max(dia_h, (int)dia_gm.gmBlackBoxY);
            w = max(w, ch_w + (int)dia_gm.gmBlackBoxX);
            dia_y = max(dia_y, ch_gm.gmptGlyphOrigin.y + (int)dia_gm.gmBlackBoxY);
        }
    }
    
    int height = max((int)ch_gm.gmBlackBoxY + dia_h + settings.vertical_spacing, 1);

    sc->width = max(w, 1);
    sc->height = height;
    sc->base_width = ch_w;
    sc->base_offset_y = dia_h + ch_gm.gmptGlyphOrigin.y;
    sc->diacritic_padding_top = dia_y;

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

#define TRY_LANG(l)\
if (!strcmp(lang_code, #l)) {\
    settings.lang = l;\
    if (settings.matrix_chars[0] == L'\0') {\
        wcscpy_s(settings.matrix_chars,\
            sizeof(settings.matrix_chars) / sizeof(wchar_t),\
            l ## _MATRIX_CHARS\
        );\
    }\
    return;\
}

static void SetLanguage(const char *lang_code) {
    TRY_LANG(EN);
    TRY_LANG(RU);
    TRY_LANG(LA);
    TRY_LANG(GR);
    TRY_LANG(JP);
    TRY_LANG(CSL);
    TRY_LANG(AKK);
    char err_msg[64];
    snprintf(err_msg, sizeof(err_msg), "Unknown language code: %s", lang_code);
    LogError(err_msg);
}

#define PARSE_INT(field)\
if (!strcmp(k, #field)) {\
    settings.field = atoi(v);\
}

#define PARSE_STR(field) \
if (!strcmp(k, #field)) { \
    size_t converted = 0; \
    mbstowcs_s(&converted, settings.field, sizeof(settings.field) / sizeof(wchar_t), v, _TRUNCATE); \
}

static void NormalizeSettings(){
    for(int i=0; i<MAX_MATRIX_CHARS; ++i){
        if(settings.matrix_chars[i] == L'\0'){
            break;
        } 
        settings.matrix_chars_count++;
    }
    if(settings.matrix_chars_count > MAX_MATRIX_CHARS) {
        settings.matrix_chars_count = MAX_MATRIX_CHARS;
    }
    if(settings.stream_speed > MAX_STREAM_SPEED) {
        settings.stream_speed = MAX_STREAM_SPEED;
    }
    if(settings.stream_count > MAX_STREAM_COUNT) {
        settings.stream_count = MAX_STREAM_COUNT;
    }
    if(settings.max_stream_length == 0){
        settings.max_stream_length = 1;
    }
    if(settings.max_stream_length > MAX_STREAM_ROWS) {
        settings.max_stream_length = MAX_STREAM_ROWS;
    }
    if(settings.min_stream_length > settings.max_stream_length){
        if(settings.max_stream_length == 1){
            settings.min_stream_length = 1;
        } else {
            settings.min_stream_length = settings.max_stream_length - 1;
        }
    }
    // settings.color_r %= 255;
    // settings.color_g %= 255;
    // settings.color_b %= 255;
}

static void LoadSettings() {
    FILE* file = OpenTextFile(L"settings.ini");

    if(file == NULL) {
        LogError("Cant open settings file\n");
    }

    // Just because custom matrix_chars could be quite long
    char line[MAX_MATRIX_CHARS + 72];
    while (fgets(line, sizeof(line), file)) {
        char k[64], v[MAX_MATRIX_CHARS];
        int scan_res = sscanf_s(
            line, 
            "%63s = %255[^\r\n]", 
            k, (unsigned)sizeof(k), 
            v, (unsigned)sizeof(v) 
        );
        if (scan_res != 2){
            char err_msg[MAX_MATRIX_CHARS + 128];
            snprintf(err_msg, sizeof(err_msg), "Can't read setting: %s", line);
            LogError(err_msg);
            continue;
        }
            
        // skip comments
        if (k[0] == '#') 
            continue;

        PARSE_INT(font_size);
        PARSE_INT(stream_count);
        PARSE_INT(stream_speed);
        PARSE_INT(min_stream_length);
        PARSE_INT(max_stream_length);
        PARSE_INT(vertical_spacing);
        PARSE_INT(whitespace_length);
        PARSE_INT(horizontal_stream_padding);
        PARSE_INT(color_r);
        PARSE_INT(color_g);
        PARSE_INT(color_b);

        PARSE_STR(file_name);
        PARSE_STR(font_familly);
        PARSE_STR(matrix_chars);

        if (!strcmp(k, "lang"))
            SetLanguage(v);
    }
    fclose(file);
    NormalizeSettings();
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
        dest[total_read].diacritics[0] = L'\0';
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
            dest[total_read].diacritics[0] = L'\0';
            total_read++;
        } else if (total_read > 0) {
            StreamCharacter prev_ch = dest[total_read - 1];
            for(int d_i = 0; d_i < N_DIACRITICS; ++d_i){
                if (prev_ch.diacritics[d_i] == L'\0') {
                    prev_ch.diacritics[d_i+1] = ch;
                    prev_ch.diacritics[d_i+1] = L'\0';
                }
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
        // Negative font_size allows CreateFontW to adjust it if needed
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

    // Calculate in advance where stream will stop
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
        BOOL lr_check = cand_right + HORIZONTAL_STREAM_PADDING > other->x;
        BOOL rl_check = other->x + other->max_width + HORIZONTAL_STREAM_PADDING > cand_left;
        if (rl_check && lr_check) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL SpawnStream(HDC hdc, Stream *stream, int screen_width, int screen_height)
{
    memset(stream->chars, 0, sizeof(stream->chars));

    size_t grabbed = 0;
    if (text_source_file != NULL) {
        grabbed = ReadTextChunk(stream->chars, MAX_STREAM_ROWS);
    }

    if (grabbed == 0) {        
        grabbed = settings.min_stream_length 
            + rand() % (settings.max_stream_length - settings.min_stream_length);
        for (size_t k = 0; k < grabbed; ++k) {
            int random_char_idx = rand() % settings.matrix_chars_count; 
            stream->chars[k].value = settings.matrix_chars[random_char_idx];
            stream->chars[k].diacritics[0] = L'\0';
        }
    }

    int sum_h = 0;
    int max_w = 0;

    for (size_t i = 0; i < grabbed; ++i) {
        MeasureStreamCharacter(hdc, &stream->chars[i]);
        sum_h += stream->chars[i].height;
        if (stream->chars[i].width > max_w) {
            max_w = stream->chars[i].width;
        }
    }

    stream->char_count = (int)grabbed;
    stream->max_width = max(max_w, 1);
    stream->max_height = sum_h;

    int max_x = max(1, screen_width - stream->max_width);
    int max_y = max(1, screen_height - 60);

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

        BOOL can_go_down = (head_bottom_y < screen_height) && 
                            (stream->head_row + 1 < stream->char_count);

        if (can_go_down) {
            ++stream->head_row;
        } else {
            ++stream->first_row;
        }

        // Calculate Y position of the top of the tail character (first_row)
        int tail_top_y = stream->start_y;
        for (int r = 0; r < stream->first_row && r < stream->char_count; ++r) {
            tail_top_y += stream->chars[r].height;
        }

        BOOL below_bottom = tail_top_y >= screen_height;
        BOOL run_off_chars = stream->first_row > stream->head_row || 
            stream->first_row >= stream->char_count;

        if (below_bottom || run_off_chars) {
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
            
            int fade = 255 - distance * 200 / count_visible;
            if (fade < 40) fade = 40;
            if (fade > 255) fade = 255;

            COLORREF font_color = RGB(
                settings.color_r * fade / 255,
                settings.color_g * fade / 255,
                settings.color_b * fade / 255
            );
            
            SetTextColor(draw_dc, font_color);

            const StreamCharacter *sc = &stream->chars[row];

            if (sc->value == L' ') {
                offset_y += sc->height;
                continue;
            }

            int ch_offset_x = (stream->max_width - sc->base_width) / 2;

            for(int d_i = 0; d_i < N_DIACRITICS; ++d_i){
                wchar_t d = sc->diacritics[d_i];
                if (d != L'\0') {
                    TextOutW(
                        draw_dc,
                        stream->x + ch_offset_x + sc->base_width,
                        offset_y + sc->diacritic_padding_top,
                        &d,
                        1
                    );
                }
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
            // LoadSettings();
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
        case WM_SETCURSOR:
        {
            SetCursor(NULL);
            return TRUE;
        }
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst) {
    return TRUE;
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

    #ifdef DEBUG_CONSOLE
    AllocConsole();
    FILE *console;
    freopen_s(&console, "CONOUT$", "w", stdout);
    #endif

    LoadSettings();

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

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}