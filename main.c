#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

#define STREAM_COUNT 300
#define STREAM_SPEED 100        // Less is faster
#define MAX_STREAM_ROWS 2048

// Build:
// cl /std:c17 /W4 /TC /utf-8 main.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib

// Run:
// main.exe


typedef struct {
    int active;
    int spawn_delay;
    int column;
    int head_row;
    int length;
    int visible_length;
    wchar_t chars[MAX_STREAM_ROWS];
} Stream;

static const wchar_t  CLASS_NAME[] = L"MatrixScreensaverWindow";
// static const wchar_t MATRIX_CHARS[] =
//     L"アイウエオカキクケコサシスセソ"
//     L"タチツテトナニヌネノハヒフヘホ"
//     L"マミムメモヤユヨラリルレロワン";

// static const wchar_t MATRIX_CHARS[] = L"АБВГДЕЖЅЗИІКЛМНОПРСТѸФХѠЦЧШЩЪЫЬѢЮꙖѤѦѨѪѬѮѰѲѴѶ";
static const wchar_t MATRIX_CHARS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
// static const wchar_t MATRIX_CHARS[] = L"ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ";

static Stream streams[STREAM_COUNT];
static int stream_cell_width = 0;
static int stream_cell_height = 0;
static int streams_initialized = 0;

// Font resource
static HFONT stream_font = NULL;

// Backbuffer
static HDC backbuffer_dc = NULL;
static HBITMAP backbuffer_bitmap = NULL;
static HBITMAP backbuffer_old_bitmap = NULL;
static int backbuffer_width = 0;
static int backbuffer_height = 0;

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
            -24,
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
            L"Yu Gothic Mono"
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

    if (!metrics_ok) {
        return FALSE;
    }

    stream_cell_width = tm.tmAveCharWidth;
    stream_cell_height = tm.tmHeight - tm.tmExternalLeading 
        - tm.tmInternalLeading - tm.tmDescent;

    if (stream_cell_width < 1) stream_cell_width = 1;
    if (stream_cell_height < 1) stream_cell_height = 1;

    return TRUE;
}

static void CleanupFont(void)
{
    if (stream_font != NULL) {
        DeleteObject(stream_font);
        stream_font = NULL;
    }
}

static void GetGridDimensions(HWND hwnd, int *out_columns, int *out_rows)
{
    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    int cols = 1;
    int rows = 1;

    if (stream_cell_width > 0) {
        cols = client_rect.right / stream_cell_width;
    }
    if (stream_cell_height > 0) {
        rows = client_rect.bottom / stream_cell_height;
    }

    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    if (cols > MAX_STREAM_ROWS) cols = MAX_STREAM_ROWS;
    if (rows > MAX_STREAM_ROWS) rows = MAX_STREAM_ROWS;

    *out_columns = cols;
    *out_rows = rows;
}

static BOOL SpawnStream(Stream *stream, int columns, int rows)
{
    if (columns <= 0 || rows <= 0) {
        return FALSE;
    }

    const int char_count =
        (int)(sizeof(MATRIX_CHARS) / sizeof(MATRIX_CHARS[0])) - 1;

    /* Try several random positions before giving up. */
    for (int attempt = 0; attempt < columns * 4; ++attempt) {
        const int column = rand() % columns;
        const int length = 8 + rand() % 13;
        const int head_row = rand() % rows;

        int first_row = head_row - length + 1;
        if (first_row < 0) {
            first_row = 0;
        }

        BOOL occupied = FALSE;

        for (int i = 0; i < STREAM_COUNT; ++i) {
            const Stream *other = &streams[i];

            if (other == stream || !other->active ||
                other->column != column) {
                continue;
            }

            int other_first = other->head_row - other->length + 1;
            if (other_first < 0) {
                other_first = 0;
            }

            int other_last = other->head_row;
            if (other_last >= rows) {
                other_last = rows - 1;
            }

            if (other_last >= 0 &&
                first_row <= other_last &&
                head_row >= other_first) {
                occupied = TRUE;
                break;
            }
        }

        if (occupied) {
            continue;
        }

        stream->column = column;
        stream->head_row = head_row;
        stream->length = length;
        stream->visible_length = 1;
        stream->active = 1;
        stream->spawn_delay = 0;

        stream->chars[head_row] = MATRIX_CHARS[rand() % char_count];

        // for (int row = 0; row <= head_row; ++row) {
        //     stream->chars[row] =
        //         MATRIX_CHARS[rand() % char_count];
        // }

        return TRUE;
    }

    return FALSE;
}

static void InitStreams(int columns, int rows)
{
    int initial_count = STREAM_COUNT;

    if (initial_count > columns) {
        initial_count = columns;
    }

    /* Start the first group immediately. */
    for (int i = 0; i < initial_count; ++i) {
        if (!SpawnStream(&streams[i], columns, rows)) {
            streams[i].active = 0;
            streams[i].spawn_delay = 1 + rand() % 20;
        }
    }

    /* Schedule the remaining streams for later. */
    for (int i = initial_count; i < STREAM_COUNT; ++i) {
        streams[i].active = 0;
        streams[i].spawn_delay = 10 + rand() % 41;
    }

    streams_initialized = 1;
}

static void UpdateStreams(int columns, int rows)
{
    if (columns < 1 || rows < 1) {
        return;
    }

    if (!streams_initialized) {
        InitStreams(columns, rows);
    }

    const int char_count =
        (int)(sizeof(MATRIX_CHARS) / sizeof(MATRIX_CHARS[0])) - 1;

    for (int i = 0; i < STREAM_COUNT; ++i) {
        Stream *stream = &streams[i];

        if (!stream->active) {
            if (stream->spawn_delay > 0) {
                --stream->spawn_delay;
            }

            if (stream->spawn_delay == 0) {
                if (!SpawnStream(stream, columns, rows)) {
                    /* Retry when a column becomes available. */
                    stream->spawn_delay = 10;
                }
            }

            continue;
        }

        ++stream->head_row;

        if (stream->head_row < rows &&
            stream->head_row < MAX_STREAM_ROWS) {
            stream->chars[stream->head_row] =
                MATRIX_CHARS[rand() % char_count];
        }

        if (stream->visible_length < stream->length) {
            ++stream->visible_length;
        }

        if (stream->head_row - stream->length + 1 >= rows) {
            stream->active = 0;
            stream->spawn_delay = 1 + rand() % 20;
        }
    }
}

static void InvalidateStream(HWND hwnd, const Stream *stream)
{
    if (stream_cell_width <= 0 || stream_cell_height <= 0) {
        return;
    }

    RECT rect;

    rect.left = stream->column * stream_cell_width;
    rect.right = rect.left + stream_cell_width + 1;

    int first_row = stream->head_row - stream->length + 1;
    if (first_row < 0) {
        first_row = 0;
    }

    rect.top = first_row * stream_cell_height;
    rect.bottom = (stream->head_row + 1) * stream_cell_height;

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

    if (stream_cell_width > 0 && stream_cell_height > 0) {
        for (int i = 0; i < STREAM_COUNT; ++i) {
            const Stream *stream = &streams[i];

            if (!stream->active) {
                continue;
            }

            const int stream_x = stream->column * stream_cell_width;

            if (stream_x >= paint_rect->right ||
                stream_x + stream_cell_width <= paint_rect->left) {
                continue;
            }

            int first_row = 
                stream->head_row - stream->visible_length + 1;

            if (first_row < 0) {
                first_row = 0;
            }

            const int visible_first_row =
                paint_rect->top / stream_cell_height;

            const int visible_last_row =
                (paint_rect->bottom - 1) / stream_cell_height;

            if (first_row < visible_first_row) {
                first_row = visible_first_row;
            }

            int last_row = stream->head_row;

            if (last_row > visible_last_row) {
                last_row = visible_last_row;
            }

            for (int row = first_row; row <= last_row; ++row) {
                const int distance = stream->head_row - row;
                const int green =
                    255 - distance * 200 / stream->visible_length;

                SetTextColor(draw_dc, RGB(0, green, 0));

                TextOutW(
                    draw_dc,
                    stream_x,
                    row * stream_cell_height,
                    &stream->chars[row],
                    1
                );
            }
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
            if (!InitFontMetrics(hwnd)) {
                return -1;
            }

            if (SetTimer(hwnd, 1, STREAM_SPEED, NULL) == 0) {
                CleanupFont();
                return -1;
            }
            return 0; 
        }       
        case WM_TIMER:
        {
            int columns = 0;
            int rows = 0;
            GetGridDimensions(hwnd, &columns, &rows);
            UpdateStreams(columns, rows);
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
        NULL);

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