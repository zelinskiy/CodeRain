#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

static const wchar_t  CLASS_NAME[] = L"MatrixScreensaverWindow";
static const wchar_t MATRIX_CHARS[] =
    L"アイウエオカキクケコサシスセソ"
    L"タチツテトナニヌネノハヒフヘホ"
    L"マミムメモヤユヨラリルレロワン"
    L"012345789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// static const COLORREF BACKGROUND_COLOR = RGB(0, 0, 0);

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {      
        case WM_PAINT:
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

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
            FillRect(
                hdc,
                &ps.rcPaint,
                (HBRUSH)GetStockObject(BLACK_BRUSH)
            );
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 255, 70));
            RECT client;
            GetClientRect(hwnd, &client);
            
            const int columns = (client.right + cell_width - 1) / cell_width;
            const int rows = (client.bottom + cell_height - 1) / cell_height;

            const size_t char_count =
                (sizeof MATRIX_CHARS / sizeof MATRIX_CHARS[0]) - 1;

            for (int col = 0; col < columns; ++col)
            {
                for (int row = 0; row < rows; ++row)
                {
                    const size_t index = (size_t)rand() % char_count;
                    const wchar_t ch = MATRIX_CHARS[index];

                    TextOutW(
                        hdc,
                        col * cell_width,
                        row * cell_height,
                        &ch,
                        1);
                }
            }

            // SelectObject(hdc, old_font);
            // DeleteObject(font);

            EndPaint(hwnd, &ps);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

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
