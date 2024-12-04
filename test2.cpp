#include <windows.h>
#include <vector>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <windowsx.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <algorithm>

// 定义控件ID
#define ID_ADD_CANVAS 1001
#define ID_TEXT_TOOL 1002
#define ID_BRUSH_TOOL 1003
#define ID_SAVE_BUTTON 1004
#define ID_COLOR_START 1100  // 颜色按钮起始ID
#define ID_THUMBNAIL_START 2000

// 定义绘画模式
enum DrawMode {
    MODE_BRUSH,
    MODE_TEXT,
    MODE_MOVE_IMAGE,
    MODE_MOVE_TEXT
};

// 预定义颜色数组
const COLORREF PRESET_COLORS[] = {
    RGB(0, 0, 0),      // 黑色
    RGB(255, 255, 255), // 白色
    RGB(128, 128, 128), // 灰色
    RGB(255, 0, 0),     // 红色
    RGB(0, 255, 0),     // 绿色
    RGB(0, 0, 255),     // 蓝色
    RGB(255, 255, 0),   // 黄色
    RGB(255, 0, 255),   // 紫色
    RGB(0, 255, 255),   // 青色
    RGB(128, 0, 0),     // 深红
    RGB(0, 128, 0),     // 深绿
    RGB(0, 0, 128),     // 深蓝
    RGB(255, 128, 0),   // 橙色
    RGB(128, 0, 255),   // 紫罗兰
    RGB(255, 192, 203)  // 粉色
};

const int COLOR_BUTTON_SIZE = 20;  // 颜色按钮大小
const int COLOR_BUTTON_MARGIN = 2; // 按钮间距
const int COLOR_BUTTON_ROWS = 2;   // 两行布局
const int COLOR_BUTTON_COLS = 8;   // 每行8个颜色
const int BUTTON_MARGIN = 5;  // 按钮间距
const int BUTTON_WIDTH = 50;  // 按钮宽度
const int BUTTON_HEIGHT = 30; // 按钮高度
const int TOOLBAR_HEIGHT = 40; // 工具栏高度

// 函数声明
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

// 全局变量
std::vector<HWND> canvases;
std::vector<HWND> thumbnails;
std::vector<HDC> memDCs;
std::vector<HBITMAP> memBitmaps;
COLORREF currentColor = RGB(0, 0, 0);
size_t currentCanvasIndex = 0;
HINSTANCE hInst;
DrawMode currentMode = MODE_BRUSH;
RECT textRect = {0};
bool isDrawingTextBox = false;
std::wstring currentText;
HWND activeEditControl = NULL;
HBITMAP hPastedBitmap = NULL;
RECT pastedImageRect = {0};
bool isMovingPastedImage = false;
POINT lastMousePos = {0};
int thumbnailScrollPos = 0;
int thumbnailTotalHeight = 0;

// 在全局变量区域添加
bool isDrawing = false;
bool isErasing = false;
POINT lastPoint = {0};
RECT eraseRect = {0};

// 函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CanvasProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ThumbnailProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void CreateToolbar(HWND hwnd);
void CreateNewCanvas(HWND parentHwnd);
void SwitchCanvas(size_t index);
void CleanupMemoryDCs();
void DrawTextOnCanvas(HDC hdc, RECT rect, const std::wstring& text);
void ResizeCanvasAndThumbnails(HWND hwnd, int width, int height);
void PasteFromClipboard(HWND hwnd);
void SaveAsLongImage(HWND hwnd);
void EraseArea(HWND hwnd, RECT eraseRect);

// GetEncoderClsid 函数实现
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

// 创建工具栏函数
void CreateToolbar(HWND hwnd)
{
    // 创建主要功能按钮
    CreateWindow(
        L"BUTTON", L"+",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        BUTTON_MARGIN, BUTTON_MARGIN, 
        BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd, (HMENU)ID_ADD_CANVAS, NULL, NULL
    );

    CreateWindow(
        L"BUTTON", L"Text",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        BUTTON_MARGIN * 2 + BUTTON_WIDTH, BUTTON_MARGIN,
        BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd, (HMENU)ID_TEXT_TOOL, NULL, NULL
    );

    CreateWindow(
        L"BUTTON", L"Pen",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        BUTTON_MARGIN * 3 + BUTTON_WIDTH * 2, BUTTON_MARGIN,
        BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd, (HMENU)ID_BRUSH_TOOL, NULL, NULL
    );

    CreateWindow(
        L"BUTTON", L"Save",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        BUTTON_MARGIN * 4 + BUTTON_WIDTH * 3, BUTTON_MARGIN,
        BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd, (HMENU)ID_SAVE_BUTTON, NULL, NULL
    );

    // 创建颜色选择按钮
    int colorStartX = BUTTON_MARGIN * 5 + BUTTON_WIDTH * 4;
    for (size_t i = 0; i < sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0]); i++)
    {
        int row = i / COLOR_BUTTON_COLS;
        int col = i % COLOR_BUTTON_COLS;
        
        CreateWindow(
            L"BUTTON", L"",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,  // 添加 BS_OWNERDRAW 样式
            colorStartX + (COLOR_BUTTON_SIZE + COLOR_BUTTON_MARGIN) * col,
            BUTTON_MARGIN + (COLOR_BUTTON_SIZE + COLOR_BUTTON_MARGIN) * row,
            COLOR_BUTTON_SIZE, COLOR_BUTTON_SIZE,
            hwnd, (HMENU)(ID_COLOR_START + i), NULL, NULL
        );
    }
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = L"MultiCanvasWindowClass";
    RegisterClassEx(&wc);

    WNDCLASSEX wcCanvas = {0};
    wcCanvas.cbSize = sizeof(WNDCLASSEX);
    wcCanvas.style = CS_HREDRAW | CS_VREDRAW;
    wcCanvas.lpfnWndProc = CanvasProc;
    wcCanvas.hInstance = hInstance;
    wcCanvas.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcCanvas.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcCanvas.lpszClassName = L"CanvasClass";
    RegisterClassEx(&wcCanvas);

    WNDCLASSEX wcThumbnail = {0};
    wcThumbnail.cbSize = sizeof(WNDCLASSEX);
    wcThumbnail.style = CS_HREDRAW | CS_VREDRAW;
    wcThumbnail.lpfnWndProc = ThumbnailProc;
    wcThumbnail.hInstance = hInstance;
    wcThumbnail.hCursor = LoadCursor(NULL, IDC_HAND);
    wcThumbnail.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wcThumbnail.lpszClassName = L"ThumbnailClass";
    RegisterClassEx(&wcThumbnail);

    HWND hwnd = CreateWindowEx(
        0,
        L"MultiCanvasWindowClass",
        L"Multi-Canvas Drawing App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CleanupMemoryDCs();

    // Shutdown GDI+
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int) msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH hBrushes[sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0])] = { NULL };

    switch (uMsg)
    {
    case WM_CREATE:
        CreateToolbar(hwnd);
        CreateNewCanvas(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) >= ID_COLOR_START && 
            LOWORD(wParam) < ID_COLOR_START + static_cast<int>(sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0])))
        {
            currentColor = PRESET_COLORS[LOWORD(wParam) - ID_COLOR_START];
            // 重绘所有颜色按钮以更新选中状态
            for (size_t i = 0; i < sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0]); i++)
            {
                HWND hColorButton = GetDlgItem(hwnd, ID_COLOR_START + i);
                InvalidateRect(hColorButton, NULL, TRUE);
            }
        }
        else
        {
            switch (LOWORD(wParam))
            {
            case ID_ADD_CANVAS:
                CreateNewCanvas(hwnd);
                break;
            case ID_TEXT_TOOL:
                currentMode = MODE_TEXT;
                break;
            case ID_BRUSH_TOOL:
                currentMode = MODE_BRUSH;
                break;
            case ID_SAVE_BUTTON:
                SaveAsLongImage(hwnd);
                break;
            default:
                if (LOWORD(wParam) >= ID_THUMBNAIL_START)
                {
                    SwitchCanvas(LOWORD(wParam) - ID_THUMBNAIL_START);
                }
                break;
            }
        }
        return 0;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            ResizeCanvasAndThumbnails(hwnd, width, height);
            
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            SCROLLINFO si = {0};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_RANGE | SIF_PAGE;
            si.nMin = 0;
            si.nMax = thumbnailTotalHeight;
            si.nPage = clientRect.bottom;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        }
        return 0;

    case WM_DESTROY:
        for (HBRUSH hBrush : hBrushes)
        {
            if (hBrush)
            {
                DeleteObject(hBrush);
            }
        }
        PostQuitMessage(0);
        return 0;
    
    case WM_VSCROLL:
        {
            SCROLLINFO si = {0};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);

            int yPos = si.nPos;
            switch (LOWORD(wParam))
            {
                case SB_LINEUP:
                    si.nPos -= 30;
                    break;
                case SB_LINEDOWN:
                    si.nPos += 30;
                    break;
                case SB_PAGEUP:
                    si.nPos -= si.nPage;
                    break;
                case SB_PAGEDOWN:
                    si.nPos += si.nPage;
                    break;
                case SB_THUMBTRACK:
                    si.nPos = si.nTrackPos;
                    break;
            }

            si.nPos = std::max(0, std::min(si.nPos, si.nMax - (int)si.nPage));

            if (si.nPos != yPos)
            {
                SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
                thumbnailScrollPos = si.nPos;
                for (size_t i = 0; i < thumbnails.size(); i++)
                {
                    SetWindowPos(thumbnails[i], NULL, 
                        10, 
                        TOOLBAR_HEIGHT + 50 + i * 110 - thumbnailScrollPos,
                        150, 100, 
                        SWP_NOZORDER);
                }
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            SendMessage(hwnd, WM_VSCROLL, 
                zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            SendMessage(hwnd, WM_VSCROLL, 
                zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            SendMessage(hwnd, WM_VSCROLL, 
                zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == 'V' && GetKeyState(VK_CONTROL) < 0)
        {
            PasteFromClipboard(canvases[currentCanvasIndex]);
        }
        else if (wParam == 'S' && GetKeyState(VK_CONTROL) < 0)
        {
            SaveAsLongImage(hwnd);
        }
        return 0;

    case WM_CTLCOLORBTN:
        {
            HDC hdc = (HDC)wParam;
            HWND hButton = (HWND)lParam;
            int buttonId = GetDlgCtrlID(hButton);

            if (buttonId >= ID_COLOR_START && buttonId < ID_COLOR_START + static_cast<int>(sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0])))
            {
                int colorIndex = buttonId - ID_COLOR_START;
                SetBkColor(hdc, PRESET_COLORS[colorIndex]);
                
                // 添加边框效果
                if (currentColor == PRESET_COLORS[colorIndex])
                {
                    RECT rect;
                    GetClientRect(hButton, &rect);
                    FrameRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    InflateRect(&rect, -1, -1);
                    FrameRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
                }

                if (!hBrushes[colorIndex])
                {
                    hBrushes[colorIndex] = CreateSolidBrush(PRESET_COLORS[colorIndex]);
                }
                return (LRESULT)hBrushes[colorIndex];
            }
        }
        break;

    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
            if (lpDIS->CtlType == ODT_BUTTON)
            {
                int colorIndex = LOWORD(lpDIS->CtlID) - ID_COLOR_START;
                if (colorIndex >= 0 && colorIndex < sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0]))
                {
                    HBRUSH hBrush = CreateSolidBrush(PRESET_COLORS[colorIndex]);
                    FillRect(lpDIS->hDC, &lpDIS->rcItem, hBrush);
                    DeleteObject(hBrush);

                    if (currentColor == PRESET_COLORS[colorIndex])
                    {
                        RECT rect = lpDIS->rcItem;
                        FrameRect(lpDIS->hDC, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        InflateRect(&rect, -1, -1);
                        FrameRect(lpDIS->hDC, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
                    }
                    return TRUE;
                }
            }
            return FALSE;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
// 继续代码...
LRESULT CALLBACK CanvasProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);  // 添加这行
        if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SPACE) < 0) {
            isErasing = true;
            eraseRect.left = GET_X_LPARAM(lParam);
            eraseRect.top = GET_Y_LPARAM(lParam);
            eraseRect.right = eraseRect.left;
            eraseRect.bottom = eraseRect.top;
        }
        else if (currentMode == MODE_BRUSH)
        {
            isDrawing = true;
            lastPoint.x = GET_X_LPARAM(lParam);
            lastPoint.y = GET_Y_LPARAM(lParam);
        }
        else if (currentMode == MODE_TEXT)
        {
            if (activeEditControl)
            {
                SendMessage(activeEditControl, WM_KILLFOCUS, 0, 0);
                activeEditControl = NULL;
            }
            isDrawingTextBox = true;
            textRect.left = GET_X_LPARAM(lParam);
            textRect.top = GET_Y_LPARAM(lParam);
            textRect.right = textRect.left;
            textRect.bottom = textRect.top;
        }
        else if (currentMode == MODE_MOVE_IMAGE)
        {
            isMovingPastedImage = true;
            lastMousePos.x = GET_X_LPARAM(lParam);
            lastMousePos.y = GET_Y_LPARAM(lParam);
        }
        return 0;

    case WM_LBUTTONUP:
        if (isErasing) {
            isErasing = false;
            // 确保矩形的方向是正确的
            if (eraseRect.right < eraseRect.left) {
                int temp = eraseRect.left;
                eraseRect.left = eraseRect.right;
                eraseRect.right = temp;
            }
            if (eraseRect.bottom < eraseRect.top) {
                int temp = eraseRect.top;
                eraseRect.top = eraseRect.bottom;
                eraseRect.bottom = temp;
            }
            EraseArea(hwnd, eraseRect);
            currentMode = MODE_BRUSH; // 回到自由绘画模式
        }
        else if (currentMode == MODE_BRUSH)
        {
            isDrawing = false;
        }
        else if (currentMode == MODE_TEXT)
        {
            isDrawingTextBox = false;
            if (textRect.right > textRect.left && textRect.bottom > textRect.top)
            {
                HWND hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                    textRect.left, textRect.top,
                    textRect.right - textRect.left, textRect.bottom - textRect.top,
                    hwnd, NULL, hInst, NULL);
                SetFocus(hEdit);
                activeEditControl = hEdit;
            }
        }
        else if (currentMode == MODE_MOVE_IMAGE)
        {
            isMovingPastedImage = false;
            size_t index = 0;
            for (HWND canvas : canvases)
            {
                if (canvas == hwnd)
                {
                    HDC hdc = GetDC(hwnd);
                    HDC hdcMem = CreateCompatibleDC(hdc);
                    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hPastedBitmap);

                    BITMAP bm;
                    GetObject(hPastedBitmap, sizeof(bm), &bm);

                    BitBlt(memDCs[index], pastedImageRect.left, pastedImageRect.top,
                           bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

                    SelectObject(hdcMem, hOldBitmap);
                    DeleteDC(hdcMem);
                    ReleaseDC(hwnd, hdc);

                    InvalidateRect(hwnd, NULL, FALSE);
                    InvalidateRect(thumbnails[index], NULL, FALSE);

                    DeleteObject(hPastedBitmap);
                    hPastedBitmap = NULL;
                    currentMode = MODE_BRUSH;
                    break;
                }
                index++;
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (isErasing) {
            eraseRect.right = GET_X_LPARAM(lParam);
            eraseRect.bottom = GET_Y_LPARAM(lParam);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (currentMode == MODE_BRUSH && isDrawing)
        {
            size_t index = 0;
            for (HWND canvas : canvases)
            {
                if (canvas == hwnd)
                {
                    HDC hdc = GetDC(hwnd);
                    HPEN hPen = CreatePen(PS_SOLID, 2, currentColor);
                    HPEN hOldPen = (HPEN)SelectObject(memDCs[index], hPen);
                    MoveToEx(memDCs[index], lastPoint.x, lastPoint.y, NULL);
                    lastPoint.x = GET_X_LPARAM(lParam);
                    lastPoint.y = GET_Y_LPARAM(lParam);
                    LineTo(memDCs[index], lastPoint.x, lastPoint.y);
                    SelectObject(memDCs[index], hOldPen);
                    DeleteObject(hPen);

                    BitBlt(hdc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 
                           memDCs[index], 0, 0, SRCCOPY);
                    ReleaseDC(hwnd, hdc);

                    InvalidateRect(thumbnails[index], NULL, FALSE);
                    break;
                }
                index++;
            }
        }
        else if (currentMode == MODE_TEXT && isDrawingTextBox)
        {
            textRect.right = GET_X_LPARAM(lParam);
            textRect.bottom = GET_Y_LPARAM(lParam);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (currentMode == MODE_MOVE_IMAGE && isMovingPastedImage)
        {
            int dx = GET_X_LPARAM(lParam) - lastMousePos.x;
            int dy = GET_Y_LPARAM(lParam) - lastMousePos.y;

            pastedImageRect.left += dx;
            pastedImageRect.top += dy;
            pastedImageRect.right += dx;
            pastedImageRect.bottom += dy;

            lastMousePos.x = GET_X_LPARAM(lParam);
            lastMousePos.y = GET_Y_LPARAM(lParam);

            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            size_t index = 0;
            for (HWND canvas : canvases)
            {
                if (canvas == hwnd)
                {
                    BitBlt(hdc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 
                           memDCs[index], 0, 0, SRCCOPY);
                    break;
                }
                index++;
            }
            
            if (currentMode == MODE_TEXT && isDrawingTextBox)
            {
                HPEN hPen = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, textRect.left, textRect.top, textRect.right, textRect.bottom);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);
            }
            else if (currentMode == MODE_MOVE_IMAGE && hPastedBitmap)
            {
                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hPastedBitmap);

                BITMAP bm;
                GetObject(hPastedBitmap, sizeof(bm), &bm);

                BitBlt(hdc, pastedImageRect.left, pastedImageRect.top,
                       bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

                SelectObject(hdcMem, hOldBitmap);
                DeleteDC(hdcMem);

                HPEN hPen = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, pastedImageRect.left, pastedImageRect.top, 
                         pastedImageRect.right, pastedImageRect.bottom);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);
            }
            else if (isErasing) {
                HPEN hPen = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, eraseRect.left, eraseRect.top, eraseRect.right, eraseRect.bottom);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);
            }

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_CTLCOLOREDIT:
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);

    case WM_SETFOCUS:
            // 当画板获得焦点时
            for (size_t i = 0; i < canvases.size(); i++)
            {
                if (canvases[i] == hwnd)
                {
                    currentCanvasIndex = i;
                    break;
                }
            }
            return 0;
    
    case WM_KILLFOCUS:
            // 当画板失去焦点时
            isDrawing = false;
            isErasing = false;
            return 0;
    
    case WM_KEYDOWN:
    case WM_KEYUP:
    // 强制重绘以更新橡皮擦状态
        if (wParam == VK_CONTROL || wParam == VK_SPACE)
        {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_KILLFOCUS)
        {
            HWND hEdit = (HWND)lParam;
            int len = GetWindowTextLength(hEdit);
            if (len > 0)
            {
                wchar_t* buffer = new wchar_t[len + 1];
                GetWindowText(hEdit, buffer, len + 1);
                currentText = buffer;
                delete[] buffer;

                size_t index = 0;
                for (HWND canvas : canvases)
                {
                    if (canvas == hwnd)
                    {
                        RECT rect;
                        GetWindowRect(hEdit, &rect);
                        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rect, 2);
                        DrawTextOnCanvas(memDCs[index], rect, currentText);
                        InvalidateRect(hwnd, NULL, FALSE);
                        InvalidateRect(thumbnails[index], NULL, FALSE);
                        break;
                    }
                    index++;
                }
            }
            DestroyWindow(hEdit);
            activeEditControl = NULL;
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ThumbnailProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            int index = GetWindowLong(hwnd, GWL_ID) - ID_THUMBNAIL_START;
            if (index >= 0 && (size_t)index < canvases.size())
            {
                RECT rect;
                GetClientRect(hwnd, &rect);
                StretchBlt(hdc, 0, 0, rect.right, rect.bottom, 
                          memDCs[index], 0, 0, 
                          GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 
                          SRCCOPY);

                bool isSelected = ((size_t)index == currentCanvasIndex);
                HPEN hPen = CreatePen(PS_SOLID, 
                    isSelected ? 3 : 1, 
                    isSelected ? RGB(0, 120, 215) : RGB(200, 200, 200)
                );
                
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, 0, 0, rect.right, rect.bottom);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_LBUTTONDOWN:
        {
            int index = GetWindowLong(hwnd, GWL_ID) - ID_THUMBNAIL_START;
            if (index >= 0 && (size_t)index < canvases.size())
            {
                SwitchCanvas(index);
            }
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateNewCanvas(HWND parentHwnd)
{
    RECT rect;
    GetClientRect(parentHwnd, &rect);
    
    // 创建画布窗口，添加 WS_CLIPSIBLINGS 样式
    HWND hwndCanvas = CreateWindowEx(
        0,
        L"CanvasClass",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,  // 添加 WS_CLIPSIBLINGS
        170, TOOLBAR_HEIGHT, rect.right - 170, rect.bottom - TOOLBAR_HEIGHT - 100,
        parentHwnd, NULL, hInst, NULL
    );
    SetFocus(hwndCanvas);

    if (hwndCanvas == NULL)
    {
        MessageBox(parentHwnd, L"Failed to create canvas window", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // 将画布设置到底层
    SetWindowPos(hwndCanvas, HWND_BOTTOM, 0, 0, 0, 0, 
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // 创建内存 DC 和位图
    HDC hdc = GetDC(hwndCanvas);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    
    if (memDC == NULL || memBitmap == NULL)
    {
        if (memDC) DeleteDC(memDC);
        if (memBitmap) DeleteObject(memBitmap);
        ReleaseDC(hwndCanvas, hdc);
        DestroyWindow(hwndCanvas);
        MessageBox(parentHwnd, L"Failed to create memory DC or bitmap", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    SelectObject(memDC, memBitmap);

    // 用白色填充画布背景
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    RECT memRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    FillRect(memDC, &memRect, whiteBrush);
    DeleteObject(whiteBrush);
    ReleaseDC(hwndCanvas, hdc);

    // 添加到容器中
    canvases.push_back(hwndCanvas);
    memDCs.push_back(memDC);
    memBitmaps.push_back(memBitmap);

    // 更新缩略图总高度
    thumbnailTotalHeight = (canvases.size() * 110) + 50;

    // 更新滚动条信息
    RECT clientRect;
    GetClientRect(parentHwnd, &clientRect);
    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = thumbnailTotalHeight;
    si.nPage = clientRect.bottom;
    SetScrollInfo(parentHwnd, SB_VERT, &si, TRUE);

    // 创建缩略图
    HWND hwndThumbnail = CreateWindowEx(
        0,
        L"ThumbnailClass",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,  // 添加 WS_CLIPSIBLINGS
        10, 
        TOOLBAR_HEIGHT + 50 + (canvases.size() - 1) * 110 - thumbnailScrollPos,
        150, 
        100,
        parentHwnd, 
        (HMENU)(ID_THUMBNAIL_START + canvases.size() - 1), 
        hInst, 
        NULL
    );

    if (hwndThumbnail == NULL)
    {
        MessageBox(parentHwnd, L"Failed to create thumbnail window", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // 将缩略图添加到容器
    thumbnails.push_back(hwndThumbnail);

    // 切换到新创建的画布
    SwitchCanvas(canvases.size() - 1);

    // 强制重绘父窗口
    InvalidateRect(parentHwnd, NULL, TRUE);
    UpdateWindow(parentHwnd);
}

void SwitchCanvas(size_t index)
{
    if (index < canvases.size())
    {
        currentCanvasIndex = index;
        for (size_t i = 0; i < canvases.size(); i++)
        {
            ShowWindow(canvases[i], i == currentCanvasIndex ? SW_SHOW : SW_HIDE);
            InvalidateRect(thumbnails[i], NULL, FALSE);
        }
        
        // 添加这行，确保新的画板获得焦点
        SetFocus(canvases[currentCanvasIndex]);
        
        InvalidateRect(canvases[currentCanvasIndex], NULL, FALSE);
    }
}

void CleanupMemoryDCs()
{
    for (size_t i = 0; i < memDCs.size(); i++)
    {
        DeleteDC(memDCs[i]);
        DeleteObject(memBitmaps[i]);
    }
    memDCs.clear();
    memBitmaps.clear();
}

void DrawTextOnCanvas(HDC hdc, RECT rect, const std::wstring& text)
{
    SetTextColor(hdc, currentColor);
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, text.c_str(), -1, &rect, DT_WORDBREAK);
}

void ResizeCanvasAndThumbnails(HWND hwnd, int width, int height)
{
    for (size_t i = 0; i < canvases.size(); i++)
    {
        SetWindowPos(canvases[i], NULL, 
            170, TOOLBAR_HEIGHT, width - 170, height - 100, 
            SWP_NOZORDER);
        InvalidateRect(canvases[i], NULL, FALSE);
    }
    
    // 重新定位所有缩略图，考虑滚动位置
    for (size_t i = 0; i < thumbnails.size(); i++)
    {
        SetWindowPos(thumbnails[i], NULL, 
            10, 
            TOOLBAR_HEIGHT + 50 + i * 110 - thumbnailScrollPos, 
            150, 100, 
            SWP_NOZORDER);
    }
}

void PasteFromClipboard(HWND hwnd)
{
    if (!OpenClipboard(hwnd))
        return;

    HANDLE hData = GetClipboardData(CF_BITMAP);
    if (hData)
    {
        HBITMAP hBitmap = (HBITMAP)CopyImage((HBITMAP)hData, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
        if (hBitmap)
        {
            if (hPastedBitmap)
            {
                DeleteObject(hPastedBitmap);
            }
            hPastedBitmap = hBitmap;

            BITMAP bm;
            GetObject(hBitmap, sizeof(bm), &bm);

            pastedImageRect.left = 0;
            pastedImageRect.top = 0;
            pastedImageRect.right = bm.bmWidth;
            pastedImageRect.bottom = bm.bmHeight;

            currentMode = MODE_MOVE_IMAGE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    else
    {
        hData = GetClipboardData(CF_UNICODETEXT);
        if (hData)
        {
            LPWSTR pText = (LPWSTR)GlobalLock(hData);
            if (pText)
            {
                currentText = pText;
                GlobalUnlock(hData);

                textRect.left = 0;
                textRect.top = 0;
                textRect.right = 200;
                textRect.bottom = 100;

                currentMode = MODE_TEXT;
                isDrawingTextBox = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
    }

    CloseClipboard();
}

void SaveAsLongImage(HWND hwnd)
{
    int totalHeight = 0;
    int maxWidth = 0;
    for (size_t i = 0; i < canvases.size(); i++)
    {
        RECT rect;
        GetClientRect(canvases[i], &rect);
        totalHeight += rect.bottom;
        maxWidth = (std::max)(maxWidth, static_cast<int>(rect.right));
    }

    HDC hdc = GetDC(hwnd);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, maxWidth, totalHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    int yOffset = 0;
    for (size_t i = 0; i < canvases.size(); i++)
    {
        RECT rect;
        GetClientRect(canvases[i], &rect);
        BitBlt(memDC, 0, yOffset, rect.right, rect.bottom, memDCs[i], 0, 0, SRCCOPY);
        yOffset += rect.bottom;
    }

    SelectObject(memDC, hOldBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd, hdc);

    OPENFILENAME ofn;
    wchar_t szFile[260] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileName(&ofn))
    {
        std::wstring fileName = ofn.lpstrFile;
        if (PathFindExtension(fileName.c_str())[0] == L'\0')
        {
            fileName += L".png";
        }

        Gdiplus::Bitmap bitmap(memBitmap, NULL);
        CLSID pngClsid;
        GetEncoderClsid(L"image/png", &pngClsid);
        bitmap.Save(fileName.c_str(), &pngClsid, NULL);
    }

    DeleteObject(memBitmap);
}

void EraseArea(HWND hwnd, RECT eraseRect) {
    size_t index = 0;
    for (HWND canvas : canvases) {
        if (canvas == hwnd) {
            HDC hdc = GetDC(hwnd);
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255)); // 白色
            FillRect(memDCs[index], &eraseRect, hBrush);
            DeleteObject(hBrush);

            BitBlt(hdc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 
                   memDCs[index], 0, 0, SRCCOPY);
            ReleaseDC(hwnd, hdc);

            InvalidateRect(thumbnails[index], NULL, FALSE);
            break;
        }
        index++;
    }
}

//最好的版本