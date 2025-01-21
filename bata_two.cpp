#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

// 手动定义属性标签和类型
#define PropertyTagComment 0x9C9C // 自定义注释属性标签
#define PropertyTagTypeASCII 2    // ASCII 字符串类型

#include <windows.h>
#include <vector>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <windowsx.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <algorithm>
#include <fstream>
#include "resource.h"

#ifndef PropertyTagTypeUnicode
#define PropertyTagTypeUnicode 7
#endif

// 定义控件ID
#define ID_ADD_CANVAS 1001
#define ID_TEXT_TOOL 1002
#define ID_BRUSH_TOOL 1003
#define ID_SAVE_BUTTON 1004
#define ID_COLOR_START 1100  // 颜色按钮起始ID
#define ID_THUMBNAIL_START 2000
#define IDM_PASTE 3001
#define IDM_CLEAR 3002

// 定义绘画模式
enum DrawMode {
    MODE_BRUSH,
    MODE_TEXT,
    MODE_MOVE_IMAGE
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
const int TOOLBAR_HEIGHT = 50; // 工具栏高度

// 函数声明
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
bool SaveBitmapWithMetadata(HBITMAP hBitmap, const std::wstring& fileName, int canvasCount);
bool LoadBitmapWithMetadata(const std::wstring& fileName, int& canvasCount);

// 全局变量
std::vector<HWND> canvases;
std::vector<HWND> thumbnails;
std::vector<HDC> memDCs;
std::vector<HBITMAP> memBitmaps;
COLORREF currentColor = RGB(0, 0, 0);
size_t currentCanvasIndex = 0;
HINSTANCE hInst;
DrawMode currentMode = MODE_BRUSH;
HBITMAP hPastedBitmap = NULL;
RECT pastedImageRect = {0};
bool isMovingPastedImage = false;
POINT lastMousePos = {0};
int thumbnailScrollPos = 0;
int thumbnailTotalHeight = 0;
int savedCanvasCount = 0; // 新增：保存时使用的画布数量

// 新增：文本输入相关变量
struct TextInput {
    std::wstring text;
    POINT position;
    bool isActive;
};
std::vector<TextInput> textInputs;
size_t currentTextInputIndex = 0;

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
void DrawTextOnCanvas(HDC hdc, const TextInput& textInput);
void ResizeCanvasAndThumbnails(HWND hwnd, int width, int height);
void PasteFromClipboard(HWND hwnd);
void SaveAsLongImage(HWND hwnd);
void EraseArea(HWND hwnd, RECT eraseRect);
void AddNewTextInput(HWND hwnd, int x, int y);
void UpdateTextInput(HWND hwnd, WPARAM wParam);
void RenderTextInputs(HWND hwnd);
void FixateTextInputs(HWND hwnd);
bool LoadAndSplitImage(HWND hwnd, const wchar_t* filename);

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
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            colorStartX + (COLOR_BUTTON_SIZE + COLOR_BUTTON_MARGIN) * col,
            BUTTON_MARGIN + (COLOR_BUTTON_SIZE + COLOR_BUTTON_MARGIN) * row,
            COLOR_BUTTON_SIZE, COLOR_BUTTON_SIZE,
            hwnd, (HMENU)(ID_COLOR_START + i), NULL, NULL
        );
    }
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
                FixateTextInputs(canvases[currentCanvasIndex]);
                break;
            case ID_SAVE_BUTTON:
                SaveAsLongImage(hwnd);
                break;
            case IDM_PASTE:
                PasteFromClipboard(canvases[currentCanvasIndex]);
                break;
            case IDM_CLEAR:
                EraseArea(canvases[currentCanvasIndex], {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)});
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

    case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            wchar_t filename[MAX_PATH];
            DragQueryFile(hDrop, 0, filename, MAX_PATH);
            DragFinish(hDrop);

            // 检查文件是否为 PNG 图片
            std::wstring fileExtension = PathFindExtension(filename);
            std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::tolower);
            if (fileExtension == L".png")
            {
                if (!LoadAndSplitImage(hwnd, filename))
                {
                    MessageBox(hwnd, L"Failed to load and split image.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
        }
        return 0;

    case WM_CONTEXTMENU:
        {
            // 获取鼠标位置
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            // 创建弹出菜单
            HMENU hMenu = CreatePopupMenu();
            if (hMenu)
            {
                // 添加菜单项
                AppendMenu(hMenu, MF_STRING, IDM_PASTE, L"粘贴");
                AppendMenu(hMenu, MF_STRING, IDM_CLEAR, L"清除");

                // 显示菜单
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON, xPos, yPos, 0, hwnd, NULL);

                // 销毁菜单
                DestroyMenu(hMenu);
            }
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

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
    SetFocus(hwnd);
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
        AddNewTextInput(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
            currentMode = MODE_BRUSH;
        }
        else if (currentMode == MODE_BRUSH)
        {
            isDrawing = false;
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
        {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            bool isMouseInCanvas = x >= 0 && x < clientRect.right && y >= 0 && y < clientRect.bottom;
            bool isLeftButtonDown = (wParam & MK_LBUTTON) != 0;

            if (isErasing) {
                if (!isLeftButtonDown) {
                    isErasing = false;
                } else if (isMouseInCanvas) {
                    eraseRect.right = x;
                    eraseRect.bottom = y;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            else if (currentMode == MODE_BRUSH)
            {
                if (!isLeftButtonDown) {
                    isDrawing = false;
                } else if (isMouseInCanvas && isDrawing) {
                    size_t index = 0;
                    for (HWND canvas : canvases)
                    {
                        if (canvas == hwnd)
                        {
                            HDC hdc = GetDC(hwnd);
                            HPEN hPen = CreatePen(PS_SOLID, 2, currentColor);
                            HPEN hOldPen = (HPEN)SelectObject(memDCs[index], hPen);
                            MoveToEx(memDCs[index], lastPoint.x, lastPoint.y, NULL);
                            lastPoint.x = x;
                            lastPoint.y = y;
                            LineTo(memDCs[index], lastPoint.x, lastPoint.y);
                            SelectObject(memDCs[index], hOldPen);
                            DeleteObject(hPen);

                            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom,
                                   memDCs[index], 0, 0, SRCCOPY);
                            ReleaseDC(hwnd, hdc);

                            InvalidateRect(thumbnails[index], NULL, FALSE);
                            break;
                        }
                        index++;
                    }
                }
            }
            else if (currentMode == MODE_MOVE_IMAGE && isMovingPastedImage)
            {
                if (!isLeftButtonDown) {
                    isMovingPastedImage = false;
                } else if (isMouseInCanvas) {
                    int dx = x - lastMousePos.x;
                    int dy = y - lastMousePos.y;

                    pastedImageRect.left += dx;
                    pastedImageRect.top += dy;
                    pastedImageRect.right += dx;
                    pastedImageRect.bottom += dy;

                    lastMousePos.x = x;
                    lastMousePos.y = y;

                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
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

            if (currentMode == MODE_MOVE_IMAGE && hPastedBitmap)
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

            RenderTextInputs(hwnd);

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_CHAR:
        if (currentMode == MODE_TEXT && !textInputs.empty() && textInputs.back().isActive)
        {
            UpdateTextInput(hwnd, wParam);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_KEYUP:
        if (wParam == VK_CONTROL || wParam == VK_SPACE)
        {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_SETFOCUS:
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
        isDrawing = false;
        isErasing = false;
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

    HWND hwndCanvas = CreateWindowEx(
        0,
        L"CanvasClass",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        170, TOOLBAR_HEIGHT, rect.right - 170, rect.bottom - TOOLBAR_HEIGHT - 0,
        parentHwnd, NULL, hInst, NULL
    );

    if (hwndCanvas == NULL)
    {
        MessageBox(parentHwnd, L"Failed to create canvas window", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    SetWindowPos(hwndCanvas, HWND_BOTTOM, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

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

    HBRUSH whiteBrush = CreateSolidBrush(RGB(192, 192, 192));
    RECT memRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    FillRect(memDC, &memRect, whiteBrush);
    DeleteObject(whiteBrush);
    ReleaseDC(hwndCanvas, hdc);

    canvases.push_back(hwndCanvas);
    memDCs.push_back(memDC);
    memBitmaps.push_back(memBitmap);

    thumbnailTotalHeight = (canvases.size() * 110) + 50;

    RECT clientRect;
    GetClientRect(parentHwnd, &clientRect);
    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = thumbnailTotalHeight;
    si.nPage = clientRect.bottom;
    SetScrollInfo(parentHwnd, SB_VERT, &si, TRUE);

    HWND hwndThumbnail = CreateWindowEx(
        0,
        L"ThumbnailClass",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
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

    thumbnails.push_back(hwndThumbnail);

    SwitchCanvas(canvases.size() - 1);

    RECT parentRect;
    GetClientRect(parentHwnd, &parentRect);

    SendMessage(parentHwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(parentRect.right, parentRect.bottom));

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

void DrawTextOnCanvas(HDC hdc, const TextInput& textInput)
{
    // 设置文本颜色
    SetTextColor(hdc, currentColor);
    SetBkMode(hdc, TRANSPARENT);

    // 查找宋体字体
    LOGFONT lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy(lf.lfFaceName, L"SimSun"); // 字体名称：宋体

    HFONT hSimSunFont = CreateFontIndirect(&lf);
    if (hSimSunFont == NULL) {
        // 如果宋体不可用，使用系统默认字体
        hSimSunFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    // 将宋体字体选入 HDC
    HFONT hOldFont = (HFONT)SelectObject(hdc, hSimSunFont);

    // 绘制文本
    std::wstring text = textInput.text;
    int x = textInput.position.x;
    int y = textInput.position.y;

    // 获取画布的右边界
    RECT canvasRect;
    GetClientRect(canvases[currentCanvasIndex], &canvasRect);
    int maxWidth = canvasRect.right - x; // 最大宽度为画布右边界减去当前 x 坐标

    std::wstring currentLine;
    std::wstring remainingText = text;

    while (!remainingText.empty())
    {
        // 找到当前行可以容纳的最大文本
        SIZE textSize;
        currentLine.clear();
        for (size_t i = 0; i < remainingText.size(); i++)
        {
            currentLine += remainingText[i];
            GetTextExtentPoint32(hdc, currentLine.c_str(), static_cast<int>(currentLine.size()), &textSize);
            if (textSize.cx > maxWidth || remainingText[i] == L'\n') // 超过宽度或遇到换行符
            {
                if (remainingText[i] == L'\n')
                {
                    // 手动换行
                    currentLine.pop_back(); // 去掉换行符
                }
                else
                {
                    // 自动换行
                    currentLine.pop_back(); // 去掉最后一个字符
                }
                break;
            }
        }

        // 绘制当前行
        RECT textRect = { x, y, x + textSize.cx, y + textSize.cy };
        DrawText(hdc, currentLine.c_str(), -1, &textRect, DT_LEFT | DT_TOP);

        // 更新剩余文本
        remainingText = remainingText.substr(currentLine.size());

        // 如果剩余文本以换行符开头，跳过换行符
        if (!remainingText.empty() && remainingText[0] == L'\n')
        {
            remainingText = remainingText.substr(1);
        }

        // 更新绘制位置
        y += textSize.cy; // 换行
    }

    // 恢复旧字体
    SelectObject(hdc, hOldFont);
    DeleteObject(hSimSunFont); // 删除宋体字体
}

void ResizeCanvasAndThumbnails(HWND hwnd, int width, int height)
{
    for (size_t i = 0; i < canvases.size(); i++)
    {
        SetWindowPos(canvases[i], NULL,
            170, TOOLBAR_HEIGHT, width - 170, height - TOOLBAR_HEIGHT - 0,
            SWP_NOZORDER);
        InvalidateRect(canvases[i], NULL, FALSE);
    }

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
                AddNewTextInput(hwnd, 0, 0);
                textInputs.back().text = pText;
                GlobalUnlock(hData);
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

        // 获取文件名和路径
        std::wstring path = fileName.substr(0, fileName.find_last_of(L"\\/") + 1);
        std::wstring name = fileName.substr(fileName.find_last_of(L"\\/") + 1);

        // 去掉扩展名
        size_t dotPos = name.find_last_of(L".");
        if (dotPos != std::wstring::npos)
        {
            name = name.substr(0, dotPos);
        }

        // 在文件名后添加 "_分割数"
        std::wstring newFileName = path + name + L"_" + std::to_wstring(canvases.size()) + L".png";

        Gdiplus::Bitmap bitmap(memBitmap, NULL);
        CLSID pngClsid;
        GetEncoderClsid(L"image/png", &pngClsid);
        bitmap.Save(newFileName.c_str(), &pngClsid, NULL);
    }

    DeleteObject(memBitmap);
}

void EraseArea(HWND hwnd, RECT eraseRect) {
    size_t index = 0;
    for (HWND canvas : canvases) {
        if (canvas == hwnd) {
            HDC hdc = GetDC(hwnd);
            HBRUSH hBrush = CreateSolidBrush(RGB(192, 192, 192));
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

void AddNewTextInput(HWND hwnd, int x, int y)
{
    TextInput newInput;
    newInput.position.x = x;
    newInput.position.y = y;
    newInput.isActive = true;
    textInputs.push_back(newInput);
    currentTextInputIndex = textInputs.size() - 1;
    InvalidateRect(hwnd, NULL, FALSE);
}

void UpdateTextInput(HWND hwnd, WPARAM wParam)
{
    if (currentTextInputIndex < textInputs.size())
    {
        TextInput& currentInput = textInputs[currentTextInputIndex];
        if (wParam == VK_BACK)
        {
            if (!currentInput.text.empty())
            {
                currentInput.text.pop_back();
            }
        }
        else if (wParam == VK_RETURN)
        {
            currentInput.text += L"\n"; // 手动换行
        }
        else
        {
            currentInput.text += static_cast<wchar_t>(wParam);
        }

        // 重新绘制
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

void RenderTextInputs(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    for (const auto& textInput : textInputs)
    {
        DrawTextOnCanvas(hdc, textInput);
    }
    ReleaseDC(hwnd, hdc);
}

void FixateTextInputs(HWND hwnd)
{
    size_t index = 0;
    for (HWND canvas : canvases)
    {
        if (canvas == hwnd)
        {
            for (const auto& textInput : textInputs)
            {
                DrawTextOnCanvas(memDCs[index], textInput);
            }
            textInputs.clear();
            currentTextInputIndex = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            InvalidateRect(thumbnails[index], NULL, FALSE);
            break;
        }
        index++;
    }
}

bool LoadAndSplitImage(HWND hwnd, const wchar_t* filename)
{
    // 清理现有的画布
    for (HWND canvas : canvases) {
        DestroyWindow(canvas);
    }
    for (HWND thumbnail : thumbnails) {
        DestroyWindow(thumbnail);
    }
    CleanupMemoryDCs();
    canvases.clear();
    thumbnails.clear();
    thumbnailScrollPos = 0;
    thumbnailTotalHeight = 0;

    // 从文件名中提取分割数
    std::wstring fileName = filename;
    size_t underscorePos = fileName.find_last_of(L"_");
    if (underscorePos == std::wstring::npos) {
        MessageBox(hwnd, L"Invalid file name format.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    size_t dotPos = fileName.find_last_of(L".");
    if (dotPos == std::wstring::npos) {
        MessageBox(hwnd, L"Invalid file name format.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring splitCountStr = fileName.substr(underscorePos + 1, dotPos - (underscorePos + 1));
    int splitCount = std::stoi(splitCountStr);

    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(filename);
    if (!bitmap) {
        MessageBox(hwnd, L"Failed to load image.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    int imageWidth = bitmap->GetWidth();
    int imageHeight = bitmap->GetHeight();
    int canvasHeight = imageHeight / splitCount;

    for (int i = 0; i < splitCount; ++i) {
        CreateNewCanvas(hwnd); // 创建新的画布和缩略图

        // 获取新画布的 HDC 和内存 DC
        HDC canvasHdc = GetDC(canvases[i]);
        HDC memDc = memDCs[i];

        // 计算当前画布需要绘制的图像部分
        int yStart = i * canvasHeight;
        int heightToDraw = std::min(canvasHeight, imageHeight - yStart);

        // 创建一个用于绘制的 GDI+ Graphics 对象
        Gdiplus::Graphics graphics(memDc);

        // 从原始图像中绘制一部分到当前画布的内存 DC
        graphics.DrawImage(bitmap, 0, 0, 0, yStart, imageWidth, heightToDraw, Gdiplus::UnitPixel);

        ReleaseDC(canvases[i], canvasHdc);
        InvalidateRect(thumbnails[i], NULL, FALSE); // 更新缩略图
    }

    delete bitmap;
    return true;
}

bool SaveBitmapWithMetadata(HBITMAP hBitmap, const std::wstring& fileName, int canvasCount)
{
    Gdiplus::Bitmap bitmap(hBitmap, NULL);
    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);

    // 将 canvasCount 转换为字符串
    std::wstring comment = L"CanvasCount=" + std::to_wstring(canvasCount);

    // 设置注释
    Gdiplus::PropertyItem propertyItem;
    propertyItem.id = PropertyTagComment; // 使用自定义注释属性标签
    propertyItem.length = (comment.size() + 1) * sizeof(wchar_t); // 确保包含 null 终止符
    propertyItem.type = PropertyTagTypeUnicode; // 使用 Unicode 字符串类型
    propertyItem.value = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(comment.c_str()));

    Gdiplus::Status status = bitmap.SetPropertyItem(&propertyItem);
    if (status != Gdiplus::Ok)
    {
        // 添加更详细的错误信息
        std::wstring errorMsg = L"Failed to set metadata. GDI+ Status: " + std::to_wstring(status);
        MessageBox(NULL, errorMsg.c_str(), L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 保存图像
    status = bitmap.Save(fileName.c_str(), &pngClsid, NULL);
    if (status != Gdiplus::Ok)
    {
        // 添加更详细的错误信息
        std::wstring errorMsg = L"Failed to save image. GDI+ Status: " + std::to_wstring(status);
        MessageBox(NULL, errorMsg.c_str(), L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

bool LoadBitmapWithMetadata(const std::wstring& fileName, int& canvasCount)
{
    Gdiplus::Bitmap bitmap(fileName.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok)
    {
        std::wstring errorMsg = L"Failed to load image for metadata retrieval.";
        MessageBox(NULL, errorMsg.c_str(), L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 获取注释大小
    UINT propertySize = bitmap.GetPropertyItemSize(PropertyTagComment); // 使用自定义注释属性标签
    if (propertySize == 0)
    {
        MessageBox(NULL, L"No metadata found in the image (GetPropertyItemSize returned 0).", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    Gdiplus::PropertyItem* propertyItem = (Gdiplus::PropertyItem*)malloc(propertySize);
    if (!propertyItem)
    {
        MessageBox(NULL, L"Failed to allocate memory for metadata.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    Gdiplus::Status status = bitmap.GetPropertyItem(PropertyTagComment, propertySize, propertyItem); // 使用自定义注释属性标签
    if (status != Gdiplus::Ok)
    {
        free(propertyItem);
        std::wstring errorMsg = L"Failed to read metadata from the image (GetPropertyItem failed). GDI+ Status: " + std::to_wstring(status);
        MessageBox(NULL, errorMsg.c_str(), L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 检查类型是否为 Unicode
    if (propertyItem->type != PropertyTagTypeUnicode) {
        free(propertyItem);
        MessageBox(NULL, L"Metadata is not in Unicode format.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 解析注释
    std::wstring comment(reinterpret_cast<wchar_t*>(propertyItem->value));
    size_t pos = comment.find(L"CanvasCount=");
    if (pos != std::wstring::npos)
    {
        try {
            canvasCount = std::stoi(comment.substr(pos + 12));
        } catch (const std::exception& e) {
            free(propertyItem);
            MessageBox(NULL, L"Failed to parse CanvasCount from metadata.", L"Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    else
    {
        free(propertyItem);
        MessageBox(NULL, L"Invalid metadata format (CanvasCount not found).", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    free(propertyItem);
    return true;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;

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
        WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 注册拖放
    DragAcceptFiles(hwnd, TRUE);

    // 处理通过命令行参数打开文件
    LPWSTR* szArglist;
    int nArgs;
    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (szArglist && nArgs > 1) {
        std::wstring filename = szArglist[1];
        LoadAndSplitImage(hwnd, filename.c_str());
    }
    LocalFree(szArglist);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CleanupMemoryDCs();

    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int) msg.wParam;
}