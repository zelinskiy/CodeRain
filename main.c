#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

#define STREAM_COUNT 10
#define MAX_STREAM_ROWS 2048

typedef struct {
    int column;
    int head_row;
    int length;
    wchar_t chars[MAX_STREAM_ROWS];
} Stream;

static const wchar_t  CLASS_NAME[] = L"MatrixScreensaverWindow";
static const wchar_t MATRIX_CHARS[] =
    L"アイウエオカキクケコサシスセソ"
    L"タチツテトナニヌネノハヒフヘホ"
    L"マミムメモヤユヨラリルレロワン";

static Stream streams[STREAM_COUNT];
static int stream_cell_width = 0;
static int stream_cell_height = 0;
static int streams_initialized = 0;

// Backbuffer
static HDC backbuffer_dc = NULL;
static HBITMAP backbuffer_bitmap = NULL;
static HBITMAP backbuffer_old_bitmap = NULL;
static int backbuffer_width = 0;
static int backbuffer_height = 0;



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

    if (backbuffer_dc != NULL) {
        if (backbuffer_old_bitmap != NULL) {
            SelectObject(backbuffer_dc, backbuffer_old_bitmap);
        }

        if (backbuffer_bitmap != NULL) {
            DeleteObject(backbuffer_bitmap);
        }

        DeleteDC(backbuffer_dc);
    }

    backbuffer_dc = NULL;
    backbuffer_bitmap = NULL;
    backbuffer_old_bitmap = NULL;
    backbuffer_width = 0;
    backbuffer_height = 0;

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


static void SpawnStream(Stream *stream, int columns, int rows)
{
    const int char_count =
        (int)(sizeof(MATRIX_CHARS) / sizeof(MATRIX_CHARS[0])) - 1;

    stream->column = rand() % columns;
    stream->head_row = rand() % rows;
    stream->length = 8 + rand() % 13;

    for (int row = 0; row <= stream->head_row; ++row) {
        stream->chars[row] = MATRIX_CHARS[rand() % char_count];
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
            if (SetTimer(hwnd, 1, 100, NULL) == 0) {
                return -1;
            }
            return 0; 
        }       
        case WM_TIMER:
        {
            RECT client_rect;
            GetClientRect(hwnd, &client_rect);

            if (stream_cell_width <= 0 || stream_cell_height <= 0) {
                return 0;
            }

            int columns = client_rect.right / stream_cell_width;
            int rows = client_rect.bottom / stream_cell_height;

            if (columns < 1) columns = 1;
            if (rows < 1) rows = 1;

            if (columns > MAX_STREAM_ROWS) columns = MAX_STREAM_ROWS;
            if (rows > MAX_STREAM_ROWS) rows = MAX_STREAM_ROWS;

            const int char_count =
                (int)(sizeof(MATRIX_CHARS) / sizeof(MATRIX_CHARS[0])) - 1;

            for (int i = 0; i < STREAM_COUNT; ++i) {
                Stream *stream = &streams[i];

                // InvalidateStream(hwnd, stream);

                ++stream->head_row;

                if (stream->head_row >= rows) {
                    SpawnStream(stream, columns, rows);
                    stream->head_row = 0;
                } else {
                    stream->chars[stream->head_row] =
                        MATRIX_CHARS[rand() % char_count];
                }

                if (stream->column >= columns) {
                    stream->column = columns - 1;
                }

                // InvalidateStream(hwnd, stream);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client_rect;
            GetClientRect(hwnd, &client_rect);

            const int width = client_rect.right;
            const int height = client_rect.bottom;

            if (!EnsureBackbuffer(hdc, width, height)) {
                EndPaint(hwnd, &ps);
                return 0;
            }

            HDC draw_dc = backbuffer_dc;
            const int saved_dc = SaveDC(draw_dc);

            if (saved_dc == 0) {
                EndPaint(hwnd, &ps);
                return 0;
            }

            IntersectClipRect(
                draw_dc,
                ps.rcPaint.left,
                ps.rcPaint.top,
                ps.rcPaint.right,
                ps.rcPaint.bottom
            );

            FillRect(
                draw_dc,
                &ps.rcPaint,
                (HBRUSH)GetStockObject(BLACK_BRUSH)
            );

            HFONT font = CreateFontW(
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

            HFONT old_font = (HFONT)SelectObject(draw_dc, font);

            TEXTMETRICW tm;

            if (!GetTextMetricsW(draw_dc, &tm))
            {
                SelectObject(draw_dc, old_font);
                DeleteObject(font);
                RestoreDC(draw_dc, saved_dc);
                EndPaint(hwnd, &ps);
                return 0;
            }

            const int cell_width = tm.tmAveCharWidth;
            const int cell_height = tm.tmHeight - tm.tmExternalLeading 
                - tm.tmInternalLeading - tm.tmDescent;
            
            stream_cell_width = cell_width;
            stream_cell_height = cell_height;

            int columns = client_rect.right / cell_width;
            int rows = client_rect.bottom / cell_height;

            if (columns < 1) columns = 1;
            if (rows < 1) rows = 1;

            if (columns > MAX_STREAM_ROWS) columns = MAX_STREAM_ROWS;
            if (rows > MAX_STREAM_ROWS) rows = MAX_STREAM_ROWS;

            if (!streams_initialized) {
                for (int i = 0; i < STREAM_COUNT; ++i) {
                    SpawnStream(&streams[i], columns, rows);
                }

                streams_initialized = 1;
            }
                        
            
            SetBkMode(draw_dc, TRANSPARENT);
            SetTextColor(draw_dc, RGB(0, 255, 70));
            RECT client;
            GetClientRect(hwnd, &client);
            
            for (int i = 0; i < STREAM_COUNT; ++i) {
                const Stream *stream = &streams[i];

                const int stream_x = stream->column * cell_width;

                if (stream_x >= ps.rcPaint.right ||
                    stream_x + cell_width <= ps.rcPaint.left) {
                    continue;
                }

                int first_row = stream->head_row - stream->length + 1;
                if (first_row < 0) {
                    first_row = 0;
                }

                const int visible_first_row =
                    ps.rcPaint.top / cell_height;

                const int visible_last_row =
                    (ps.rcPaint.bottom - 1) / cell_height;

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
                        255 - distance * 200 / stream->length;

                    SetTextColor(draw_dc, RGB(0, green, 0));

                    TextOutW(
                        draw_dc,
                        stream_x,
                        row * cell_height,
                        &stream->chars[row],
                        1
                    );
                }
            }

            RestoreDC(draw_dc, saved_dc);

            const int paint_width = ps.rcPaint.right - ps.rcPaint.left;
            const int paint_height = ps.rcPaint.bottom - ps.rcPaint.top;

            if (paint_width > 0 && paint_height > 0) {
                BitBlt(
                    hdc,
                    ps.rcPaint.left,
                    ps.rcPaint.top,
                    paint_width,
                    paint_height,
                    backbuffer_dc,
                    ps.rcPaint.left,
                    ps.rcPaint.top,
                    SRCCOPY
                );
            }

            DeleteObject(font);
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
            if (backbuffer_dc != NULL) {
                if (backbuffer_old_bitmap != NULL) {
                    SelectObject(backbuffer_dc, backbuffer_old_bitmap);
                }

                if (backbuffer_bitmap != NULL) {
                    DeleteObject(backbuffer_bitmap);
                }

                DeleteDC(backbuffer_dc);

                backbuffer_dc = NULL;
                backbuffer_bitmap = NULL;
                backbuffer_old_bitmap = NULL;
}
            KillTimer(hwnd, 1);
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
