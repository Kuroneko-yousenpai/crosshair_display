#include <windows.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

#define DEBUG_MODE 0
#define IDB_PNG1 129

using namespace Gdiplus;

HINSTANCE hInstance;
ULONG_PTR gdiplusToken;
HBITMAP hBitmap = nullptr;
POINT imagePos = { 100, 100 };
SIZE imageSize = { 0, 0 };
bool isDragging = false;
bool isVisible = false;
POINT dragOffset = { 0, 0 };
HWND hwndImage = nullptr;

HBITMAP LoadTransparentBitmapFromResource(int resourceId, HDC hdc, SIZE* size) {
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), L"PNG");
    if (!hResource) return nullptr;

    HGLOBAL hLoadedResource = LoadResource(hInstance, hResource);
    if (!hLoadedResource) return nullptr;

    void* pResourceData = LockResource(hLoadedResource);
    DWORD resourceSize = SizeofResource(hInstance, hResource);
    if (!pResourceData || resourceSize == 0) return nullptr;

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, resourceSize);
    if (!hBuffer) return nullptr;

    void* pBuffer = GlobalLock(hBuffer);
    if (!pBuffer) {
        GlobalFree(hBuffer);
        return nullptr;
    }
    memcpy(pBuffer, pResourceData, resourceSize);
    GlobalUnlock(hBuffer);

    IStream* pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hBuffer, TRUE, &pStream))) {
        GlobalFree(hBuffer);
        return nullptr;
    }

    Bitmap bitmap(pStream);
    pStream->Release();

    if (bitmap.GetLastStatus() != Ok) return nullptr;

    size->cx = bitmap.GetWidth();
    size->cy = bitmap.GetHeight();

    HBITMAP hBitmap = nullptr;
    bitmap.GetHBITMAP(0, &hBitmap);

    return hBitmap;
}

void DrawDraggableRegion(HDC hdc, POINT position, SIZE size) {
#if DEBUG_MODE
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0)); // Red pen
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent brush
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);

    int centerX = size.cx / 2;
    int centerY = size.cy / 2;

    // Define the rectangle coordinates
    RECT draggableRect = {
        centerX - 16,
        centerY - 10,
        centerX + 20,
        centerY + 25
    };

    // Draw the rectangle
    Rectangle(hdc, draggableRect.left, draggableRect.top, draggableRect.right, draggableRect.bottom);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(hPen);
#endif
}

void UpdateWindow(HWND hwnd, HBITMAP hBitmap, SIZE size, POINT position) {
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Draw the draggable region directly on the memory DC
    DrawDraggableRegion(hdcMem, position, size);

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT zero = { 0, 0 };

    // Update the layered window
    UpdateLayeredWindow(hwnd, hdcScreen, &position, &size, hdcMem, &zero, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void SetWindowTransparent(HWND hwnd, bool transparent) {
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (transparent) {
        SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_TRANSPARENT);
    }
    else {
        SetWindowLong(hwnd, GWL_EXSTYLE, style & ~WS_EX_TRANSPARENT);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_LBUTTONDOWN: {
        POINT cursorPos;
        GetCursorPos(&cursorPos);

        // Define the central draggable area
        RECT centralRect = {
            imagePos.x + (imageSize.cx / 2) - 16,
            imagePos.y + (imageSize.cy / 2) - 10,
            imagePos.x + (imageSize.cx / 2) + 20,
            imagePos.y + (imageSize.cy / 2) + 25
        };

        if (PtInRect(&centralRect, cursorPos)) {
            isDragging = true;
            dragOffset.x = cursorPos.x - imagePos.x;
            dragOffset.y = cursorPos.y - imagePos.y;
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (isDragging) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);

            imagePos.x = cursorPos.x - dragOffset.x;
            imagePos.y = cursorPos.y - dragOffset.y;

            UpdateWindow(hwnd, hBitmap, imageSize, imagePos);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        isDragging = false;
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ToggleImageVisibility(HWND hwndParent) {
    if (isVisible) {
        ShowWindow(hwndImage, SW_HIDE);
        isVisible = false;
    }
    else {
        POINT cursorPos;
        GetCursorPos(&cursorPos);

        imagePos.x = cursorPos.x - (imageSize.cx / 2);
        imagePos.y = cursorPos.y - (imageSize.cy / 2);

        UpdateWindow(hwndImage, hBitmap, imageSize, imagePos);

        ShowWindow(hwndImage, SW_SHOW);
        isVisible = true;
    }
}

int APIENTRY WinMain(HINSTANCE hInstance_, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInstance = hInstance_;

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const wchar_t CLASS_NAME[] = L"TransparentImageApp";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    hwndImage = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"Transparent Image App",
        WS_POPUP,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwndImage) {
        return 0;
    }

    HDC hdc = GetDC(nullptr);
    hBitmap = LoadTransparentBitmapFromResource(IDB_PNG1, hdc, &imageSize);
    ReleaseDC(nullptr, hdc);

    if (!hBitmap) {
        MessageBox(hwndImage, L"Failed to load image from resources", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Set initial size and position
    UpdateWindow(hwndImage, hBitmap, imageSize, imagePos);

    // Register the hotkey (Ctrl + Numpad Dot)
    if (!RegisterHotKey(nullptr, 1, MOD_CONTROL, VK_DECIMAL)) {
        MessageBox(nullptr, L"Failed to register hotkey", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Register the hotkey (Ctrl + /)
    if (!RegisterHotKey(nullptr, 2, MOD_CONTROL, VK_DIVIDE)) {
        MessageBox(nullptr, L"Failed to register hotkey", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 1) { // Hotkey ID 1 (Ctrl + Numpad Dot)
                ToggleImageVisibility(hwndImage);
            }
            else if (msg.wParam == 2) { // Hotkey ID 2 (Ctrl + /)
                LONG style = GetWindowLong(hwndImage, GWL_EXSTYLE);
                bool transparent = style & WS_EX_TRANSPARENT;
                SetWindowTransparent(hwndImage, !transparent);
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(nullptr, 1);

    DeleteObject(hBitmap);
    GdiplusShutdown(gdiplusToken);

    return 0;
}
