#include "common.h"
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cstring>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "features.h"
#include "ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::string g_dir;
static HMODULE g_self = nullptr;
static volatile bool g_menu = false;
static bool g_inited = false;
static HWND g_hwnd = nullptr;
static WNDPROC g_oldWndProc = nullptr;
static bool g_unicodeWnd = false;
static IDirect3DDevice9* g_device = nullptr;

// ---------------------------------------------------------------- log
void Log(const char* fmt, ...) {
  std::string p = g_dir + "RE5Overlay.log";
  FILE* f = fopen(p.c_str(), "a");
  if (!f) return;
  SYSTEMTIME t; GetLocalTime(&t);
  fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
  va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
  fputc('\n', f); fclose(f);
}

// ---------------------------------------------------------------- hooks de cursor / dinput
typedef BOOL (WINAPI* SetCursorPos_t)(int, int);
typedef BOOL (WINAPI* ClipCursor_t)(const RECT*);
static SetCursorPos_t oSetCursorPos = nullptr;
static ClipCursor_t   oClipCursor = nullptr;
static BOOL WINAPI hkSetCursorPos(int x, int y) { if (g_menu) return TRUE; return oSetCursorPos(x, y); }
static BOOL WINAPI hkClipCursor(const RECT* r) { if (g_menu) return oClipCursor(nullptr); return oClipCursor(r); }

typedef HRESULT (WINAPI* GDS_t)(void*, DWORD, LPVOID);
typedef HRESULT (WINAPI* GDD_t)(void*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
static GDS_t oGDS_A = nullptr, oGDS_W = nullptr;
static GDD_t oGDD_A = nullptr, oGDD_W = nullptr;

static void BlockState(DWORD cb, LPVOID d) { if (d && (cb == 256 || cb == 16 || cb == 20)) memset(d, 0, cb); }
static HRESULT WINAPI hkGDS_A(void* s, DWORD cb, LPVOID d) { HRESULT hr = oGDS_A(s, cb, d); if (g_menu && SUCCEEDED(hr)) BlockState(cb, d); return hr; }
static HRESULT WINAPI hkGDS_W(void* s, DWORD cb, LPVOID d) { HRESULT hr = oGDS_W(s, cb, d); if (g_menu && SUCCEEDED(hr)) BlockState(cb, d); return hr; }
static HRESULT WINAPI hkGDD_A(void* s, DWORD cb, LPDIDEVICEOBJECTDATA d, LPDWORD n, DWORD f) {
  HRESULT hr = oGDD_A(s, cb, d, n, f); if (g_menu && SUCCEEDED(hr) && n && !(f & DIGDD_PEEK)) *n = 0; return hr; }
static HRESULT WINAPI hkGDD_W(void* s, DWORD cb, LPDIDEVICEOBJECTDATA d, LPDWORD n, DWORD f) {
  HRESULT hr = oGDD_W(s, cb, d, n, f); if (g_menu && SUCCEEDED(hr) && n && !(f & DIGDD_PEEK)) *n = 0; return hr; }

static void HookDInput() {
  HMODULE h = LoadLibraryA("dinput8.dll");
  if (!h) return;
  typedef HRESULT (WINAPI* Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
  Create_t create = (Create_t)GetProcAddress(h, "DirectInput8Create");
  if (!create) return;
  HINSTANCE inst = GetModuleHandleA(nullptr);

  IDirectInput8A* a = nullptr; IDirectInputDevice8A* da = nullptr;
  if (SUCCEEDED(create(inst, 0x0800, IID_IDirectInput8A, (LPVOID*)&a, nullptr)) && a &&
      SUCCEEDED(a->CreateDevice(GUID_SysMouse, &da, nullptr)) && da) {
    void** vt = *(void***)da;
    MH_CreateHook(vt[9],  (void*)hkGDS_A, (void**)&oGDS_A);
    MH_CreateHook(vt[10], (void*)hkGDD_A, (void**)&oGDD_A);
    da->Release();
  }
  if (a) a->Release();

  IDirectInput8W* w = nullptr; IDirectInputDevice8W* dw = nullptr;
  if (SUCCEEDED(create(inst, 0x0800, IID_IDirectInput8W, (LPVOID*)&w, nullptr)) && w &&
      SUCCEEDED(w->CreateDevice(GUID_SysMouse, &dw, nullptr)) && dw) {
    void** vt = *(void***)dw;
    MH_CreateHook(vt[9],  (void*)hkGDS_W, (void**)&oGDS_W);   // pode retornar "ja criado": ignorado
    MH_CreateHook(vt[10], (void*)hkGDD_W, (void**)&oGDD_W);
    dw->Release();
  }
  if (w) w->Release();
}

// ---------------------------------------------------------------- WndProc
static LRESULT CALLBACK HookedWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (g_menu) {
    ImGui_ImplWin32_WndProcHandler(h, m, w, l);
    switch (m) {
      case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
      case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
      case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
      case WM_XBUTTONDOWN: case WM_XBUTTONUP:
      case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: case WM_MOUSEMOVE:
      case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_CHAR:
        return 0;
      case WM_INPUT:
        return DefWindowProcW(h, m, w, l);
      case WM_SETCURSOR:
        return TRUE;
    }
  }
  return g_unicodeWnd ? CallWindowProcW(g_oldWndProc, h, m, w, l) : CallWindowProcA(g_oldWndProc, h, m, w, l);
}

// ---------------------------------------------------------------- menu / atalhos
static void ToggleMenu() {
  g_menu = !g_menu;
  ImGuiIO& io = ImGui::GetIO();
  if (g_menu) {
    io.MouseDrawCursor = true;
    if (oClipCursor) oClipCursor(nullptr);
    Log("menu aberto");
  } else {
    io.MouseDrawCursor = false;
    if (g_cfgDirty) Config_Save();
    Log("menu fechado");
  }
}

static bool ForegroundIsUs() {
  HWND fg = GetForegroundWindow();
  if (!fg) return false;
  DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
  return pid == GetCurrentProcessId();
}

static void PollHotkeys() {
  static bool prev[256] = {};
  bool cur[256] = {};
  if (ForegroundIsUs())
    for (int i = 1; i < 256; ++i) cur[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
  if (!g_binding) {
    int mk = g_menuKey;
    if (mk > 0 && mk < 256 && cur[mk] && !prev[mk]) ToggleMenu();
    for (auto& f : g_features) {
      if (f.vk > 0 && f.vk < 256 && cur[f.vk] && !prev[f.vk]) {
        if (f.kind == Kind::Toggle) Features_Toggle(f); else Features_Trigger(f);
      }
    }
  }
  memcpy(prev, cur, sizeof prev);
}

// ---------------------------------------------------------------- D3D9
typedef HRESULT (WINAPI* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT (WINAPI* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static Present_t oPresent = nullptr;
static Reset_t   oReset = nullptr;

static void InitImGui(IDirect3DDevice9* dev) {
  D3DDEVICE_CREATION_PARAMETERS cp = {};
  dev->GetCreationParameters(&cp);
  g_hwnd = cp.hFocusWindow ? cp.hFocusWindow : GetForegroundWindow();
  g_device = dev;
  Log("device=%p hwnd=%p", dev, g_hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr; io.LogFilename = nullptr;
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  char win[MAX_PATH]; GetWindowsDirectoryA(win, MAX_PATH);
  std::string font = std::string(win) + "\\Fonts\\segoeui.ttf";
  if (GetFileAttributesA(font.c_str()) != INVALID_FILE_ATTRIBUTES)
    io.Fonts->AddFontFromFileTTF(font.c_str(), 17.f, nullptr, io.Fonts->GetGlyphRangesDefault());
  UI_Style();
  ImGui_ImplWin32_Init(g_hwnd);
  ImGui_ImplDX9_Init(dev);

  g_unicodeWnd = IsWindowUnicode(g_hwnd) != FALSE;
  g_oldWndProc = g_unicodeWnd
      ? (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc)
      : (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
  Features_ApplyPending();
  g_inited = true;
  Log("overlay inicializado");
}

static void RenderMenu(IDirect3DDevice9* dev) {
  ImGui_ImplDX9_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  UI_Draw();
  ImGui::Render();

  IDirect3DSurface9* oldRT = nullptr; IDirect3DSurface9* bb = nullptr;
  dev->GetRenderTarget(0, &oldRT);
  dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
  if (bb) dev->SetRenderTarget(0, bb);
  if (SUCCEEDED(dev->BeginScene())) {
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    dev->EndScene();
  }
  if (oldRT) { dev->SetRenderTarget(0, oldRT); oldRT->Release(); }
  if (bb) bb->Release();
}

static HRESULT WINAPI hkPresent(IDirect3DDevice9* dev, const RECT* a, const RECT* b, HWND w, const RGNDATA* c) {
  if (!g_inited) InitImGui(dev);
  if (dev == g_device) {
    PollHotkeys();
    Features_Tick();
    if (g_menu) RenderMenu(dev);
  }
  return oPresent(dev, a, b, w, c);
}

static HRESULT WINAPI hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
  if (g_inited && dev == g_device) ImGui_ImplDX9_InvalidateDeviceObjects();
  HRESULT hr = oReset(dev, pp);
  if (g_inited && dev == g_device && SUCCEEDED(hr)) ImGui_ImplDX9_CreateDeviceObjects();
  return hr;
}

static bool GetD3D9VTable(void** table /*119*/) {
  WNDCLASSEXA wc = {}; wc.cbSize = sizeof wc; wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "RE5OvDummy";
  RegisterClassExA(&wc);
  HWND w = CreateWindowExA(0, wc.lpszClassName, "d", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
  bool ok = false;
  IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (d3d && w) {
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.hDeviceWindow = w;
    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, w, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    if (SUCCEEDED(hr) && dev) {
      memcpy(table, *(void***)dev, 119 * sizeof(void*));
      dev->Release(); ok = true;
    } else Log("CreateDevice falhou: 0x%08X", (unsigned)hr);
  }
  if (d3d) d3d->Release();
  if (w) DestroyWindow(w);
  UnregisterClassA(wc.lpszClassName, wc.hInstance);
  return ok;
}

static DWORD WINAPI InitThread(LPVOID) {
  { std::string p = g_dir + "RE5Overlay.log"; DeleteFileA(p.c_str()); }
  Log("RE5 Overlay carregado (pid %lu)", GetCurrentProcessId());
  Features_Init();
  Features_LoadOffsets();
  Config_Load();

  if (MH_Initialize() != MH_OK) { Log("MH_Initialize falhou"); return 0; }
  void* vt[119] = {};
  bool ok = false;
  for (int i = 0; i < 60 && !ok; ++i) { ok = GetD3D9VTable(vt); if (!ok) Sleep(1000); }
  if (!ok) { Log("nao foi possivel obter a vtable do D3D9"); return 0; }

  MH_STATUS s1 = MH_CreateHook(vt[17], (void*)hkPresent, (void**)&oPresent);
  MH_STATUS s2 = MH_CreateHook(vt[16], (void*)hkReset,   (void**)&oReset);
  MH_CreateHookApi(L"user32", "SetCursorPos", (void*)hkSetCursorPos, (void**)&oSetCursorPos);
  MH_CreateHookApi(L"user32", "ClipCursor",   (void*)hkClipCursor,   (void**)&oClipCursor);
  HookDInput();
  MH_STATUS s3 = MH_EnableHook(MH_ALL_HOOKS);
  Log("hooks: Present=%d Reset=%d Enable=%d", (int)s1, (int)s2, (int)s3);
  return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = h;
    DisableThreadLibraryCalls(h);
    char p[MAX_PATH]; GetModuleFileNameA(h, p, MAX_PATH);
    g_dir = p; g_dir = g_dir.substr(0, g_dir.find_last_of("\\/") + 1);
    HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
  }
  return TRUE;
}
