#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

#define STREAM_COUNT 32
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
    L"マミムメモヤユヨラリルレロワン"
    L"012345789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static Stream streams[STREAM_COUNT];
static int stream_cell_width = 0;
static int stream_cell_height = 0;
static int streams_initialized = 0;

// static const COLORREF BACKGROUND_COLOR = RGB(0, 0, 0);

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

            HFONT old_font = (HFONT)SelectObject(hdc, font);

            TEXTMETRICW tm;

            if (!GetTextMetricsW(hdc, &tm))
            {
                SelectObject(hdc, old_font);
                DeleteObject(font);
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
            

            FillRect(
                hdc,
                &ps.rcPaint,
                (HBRUSH)GetStockObject(BLACK_BRUSH)
            );
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 255, 70));
            RECT client;
            GetClientRect(hwnd, &client);
            
            for (int i = 0; i < STREAM_COUNT; ++i) {
                const Stream *stream = &streams[i];

                int first_row = stream->head_row - stream->length + 1;

                if (first_row < 0) {
                    first_row = 0;
                }

                for (int row = first_row; row <= stream->head_row; ++row) {
                    const int distance = stream->head_row - row;
                    const int green = 255 - distance * 200 / stream->length;

                    SetTextColor(hdc, RGB(0, green, 0));

                    TextOutW(
                        hdc,
                        stream->column * cell_width,
                        row * cell_height,
                        &stream->chars[row],
                        1
                    );
                }
            }

            SelectObject(hdc, old_font);
            DeleteObject(font);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
        {
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
