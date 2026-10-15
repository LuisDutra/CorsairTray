#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <cwchar>
#include <vector>

#include "iCUESDK.h"

namespace
{
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_REFRESH = WM_APP + 2;
constexpr UINT_PTR TIMER_POLL = 1;
constexpr UINT_PTR TIMER_RETRY_ADD = 2;
constexpr UINT POLL_INTERVAL_MS = 30 * 1000;
constexpr UINT RETRY_ADD_INTERVAL_MS = 2 * 1000;
constexpr UINT ID_MENU_REFRESH = 100;
constexpr UINT ID_MENU_EXIT = 101;

HWND g_hwnd = nullptr;
UINT g_taskbarCreated = 0;
HICON g_icon = nullptr;
bool g_iconAdded = false;
std::atomic<bool> g_connected{false}; // escrito pela thread do SDK

struct LevelStyle
{
    COLORREF background;
    COLORREF text;
};

// 100 verde, <100 azul, <50 amarelo, <20 vermelho, desconhecido cinza.
LevelStyle styleForLevel(int level)
{
    const COLORREF white = RGB(255, 255, 255);
    if (level < 0)
        return {RGB(110, 110, 110), white};
    if (level < 20)
        return {RGB(220, 40, 40), white};
    if (level < 50)
        return {RGB(240, 200, 0), RGB(0, 0, 0)}; // texto preto para contrastar com amarelo
    if (level < 100)
        return {RGB(30, 110, 230), white};
    return {RGB(40, 170, 70), white};
}

// Chamado pela thread do SDK: apenas repassa o evento para a janela.
void onSessionStateChanged(void *, const CorsairSessionStateChanged *s)
{
    if (!s)
        return;
    g_connected = (s->state == CSS_Connected);
    if (g_hwnd)
        PostMessageW(g_hwnd, WM_REFRESH, 0, 0);
}

// Retorna o nivel (0-100) do primeiro headset com bateria legivel, ou -1.
int readHeadsetBattery()
{
    if (!g_connected)
        return -1;

    CorsairDeviceFilter filter{};
    filter.deviceTypeMask = CDT_Headset;

    CorsairDeviceInfo devices[CORSAIR_DEVICE_COUNT_MAX];
    int count = 0;
    if (CorsairGetDevices(&filter, CORSAIR_DEVICE_COUNT_MAX, devices, &count) != CE_Success)
        return -1;

    for (int i = 0; i < count; ++i)
    {
        CorsairProperty p{};
        if (CorsairReadDeviceProperty(devices[i].id, CDPI_BatteryLevel, 0, &p) != CE_Success)
            continue;
        int level = (p.type == CT_Int32) ? p.value.int32 : -1;
        CorsairFreeProperty(&p);
        if (level >= 0)
            return level > 100 ? 100 : level;
    }
    return -1;
}

HICON makeIcon(int level)
{
    // Tamanho real do icone da bandeja (16px a 100%, maior em DPI alto).
    const int size = GetSystemMetrics(SM_CXSMICON);

    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, size, size);
    ReleaseDC(nullptr, screen);

    // Mascara monocromatica toda zerada = icone totalmente opaco.
    // Linhas de bitmap monocromatico sao alinhadas em WORD.
    const int stride = ((size + 15) / 16) * 2;
    std::vector<BYTE> maskBits(static_cast<size_t>(stride) * size, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());

    LevelStyle style = styleForLevel(level);
    wchar_t text[8];
    if (level < 0)
        wcscpy_s(text, L"?");
    else
        swprintf_s(text, L"%d", level);

    HGDIOBJ oldBmp = SelectObject(dc, color);
    HBRUSH brush = CreateSolidBrush(style.background);
    RECT rc{0, 0, size, size};
    FillRect(dc, &rc, brush);
    DeleteObject(brush);

    // "100" precisa de fonte menor e condensada para caber no icone.
    const bool threeDigits = wcslen(text) >= 3;
    const int fontHeight = threeDigits ? size * 7 / 8 : size;
    const int fontWidth = threeDigits ? size * 5 / 16 : 0;
    // Antialias em escala de cinza: ClearType gera franjas coloridas no bitmap do icone.
    HFONT font = CreateFontW(-fontHeight, fontWidth, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, style.text);
    DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBmp);
    DeleteDC(dc);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

void updateTray()
{
    int level = readHeadsetBattery();

    HICON newIcon = makeIcon(level);

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = newIcon;
    if (level >= 0)
        swprintf_s(nid.szTip, L"Headset: %d%%", level);
    else
        wcscpy_s(nid.szTip, g_connected ? L"Headset nao encontrado" : L"iCUE desconectado");

    if (g_iconAdded)
    {
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
    else
    {
        // No login o Explorer pode ainda nao estar pronto: tenta de novo depois.
        g_iconAdded = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
        if (g_iconAdded)
            KillTimer(g_hwnd, TIMER_RETRY_ADD);
        else
            SetTimer(g_hwnd, TIMER_RETRY_ADD, RETRY_ADD_INTERVAL_MS, nullptr);
    }

    if (g_icon)
        DestroyIcon(g_icon);
    g_icon = newIcon;
}

void showMenu()
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_MENU_REFRESH, L"Atualizar");
    AppendMenuW(menu, MF_STRING, ID_MENU_EXIT, L"Sair");

    POINT pt;
    GetCursorPos(&pt);
    // SetForegroundWindow + WM_NULL: necessario para o menu fechar ao clicar fora
    // e abrir corretamente na segunda vez (ver docs do TrackPopupMenu).
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_taskbarCreated) // explorer reiniciou: o icone precisa ser readicionado
    {
        g_iconAdded = false;
        updateTray();
        return 0;
    }

    switch (msg)
    {
    case WM_TIMER:
        if (wp == TIMER_POLL || wp == TIMER_RETRY_ADD)
            updateTray();
        return 0;
    case WM_REFRESH:
        updateTray();
        return 0;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU)
            showMenu();
        else if (lp == WM_LBUTTONDBLCLK)
            updateTray();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_MENU_REFRESH)
            updateTray();
        else if (LOWORD(wp) == ID_MENU_EXIT)
            DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
    {
        KillTimer(hwnd, TIMER_POLL);
        KillTimer(hwnd, TIMER_RETRY_ADD);
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    // instancia unica
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"CorsairTray.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    // Sem isso o Windows reporta 16px e amplia o icone (fica borrado em DPI alto).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CorsairTrayWnd";
    RegisterClassW(&wc);

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    // Janela top-level nunca exibida (nao HWND_MESSAGE): janelas message-only nao
    // recebem o broadcast TaskbarCreated nem funcionam bem com SetForegroundWindow.
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"CorsairTray", WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd)
        return 1;

    updateTray(); // icone cinza ate o iCUE conectar
    SetTimer(g_hwnd, TIMER_POLL, POLL_INTERVAL_MS, nullptr);

    CorsairConnect(&onSessionStateChanged, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    CorsairDisconnect();
    if (g_icon)
        DestroyIcon(g_icon);
    CloseHandle(mutex);
    return 0;
}
