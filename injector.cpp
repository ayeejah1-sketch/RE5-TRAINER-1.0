#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#pragma comment(linker, "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")

static const wchar_t* kGameExe = L"re5dx9.exe";
static const wchar_t* kDllName = L"RE5Overlay.dll";

static DWORD FindPid() {
  DWORD pid = 0;
  HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (s == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32W e; e.dwSize = sizeof e;
  if (Process32FirstW(s, &e)) do { if (!_wcsicmp(e.szExeFile, kGameExe)) { pid = e.th32ProcessID; break; } } while (Process32NextW(s, &e));
  CloseHandle(s);
  return pid;
}

static bool HasModule(DWORD pid, const wchar_t* name) {
  bool found = false;
  HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (s == INVALID_HANDLE_VALUE) return false;
  MODULEENTRY32W e; e.dwSize = sizeof e;
  if (Module32FirstW(s, &e)) do { if (!_wcsicmp(e.szModule, name)) { found = true; break; } } while (Module32NextW(s, &e));
  CloseHandle(s);
  return found;
}

static bool Inject(DWORD pid, const std::wstring& dll) {
  HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                         PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
  if (!h) { printf("  Falha ao abrir o processo (erro %lu).\n", GetLastError()); return false; }

  BOOL wow = FALSE; IsWow64Process(h, &wow);
  SYSTEM_INFO si; GetNativeSystemInfo(&si);
  if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 && !wow) {
    printf("  O jogo e de 64 bits; este trainer e para a versao de 32 bits.\n"); CloseHandle(h); return false;
  }
  size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
  LPVOID mem = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!mem || !WriteProcessMemory(h, mem, dll.c_str(), bytes, nullptr)) {
    printf("  Falha ao escrever no processo (erro %lu).\n", GetLastError()); CloseHandle(h); return false;
  }
  LPTHREAD_START_ROUTINE ll = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  HANDLE t = CreateRemoteThread(h, nullptr, 0, ll, mem, 0, nullptr);
  if (!t) { printf("  Falha ao criar thread remota (erro %lu).\n", GetLastError()); CloseHandle(h); return false; }
  WaitForSingleObject(t, 15000);
  DWORD code = 0; GetExitCodeThread(t, &code);
  CloseHandle(t); VirtualFreeEx(h, mem, 0, MEM_RELEASE); CloseHandle(h);
  if (!code) { printf("  A DLL nao carregou (confira se RE5Overlay.dll esta na mesma pasta).\n"); return false; }
  return true;
}

int main() {
  SetConsoleOutputCP(65001);
  SetConsoleTitleW(L"RE5 Ultimate Trainer");
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring dir = path; dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
  std::wstring dll = dir + kDllName;

  printf("=== RE5 Ultimate Trainer ===\n");
  if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
    printf("ERRO: RE5Overlay.dll nao encontrada ao lado deste programa.\n");
    printf("Mantenha todos os arquivos extraidos juntos na mesma pasta.\n");
    system("pause"); return 1;
  }
  printf("Deixe esta janela aberta. Abra o Resident Evil 5 e aperte F8 no jogo.\n\n");

  bool waitingMsg = false;
  for (;;) {
    DWORD pid = FindPid();
    if (!pid) {
      if (!waitingMsg) { printf("Aguardando o jogo (re5dx9.exe)...\n"); waitingMsg = true; }
      Sleep(1000); continue;
    }
    waitingMsg = false;
    printf("Jogo detectado (PID %lu). Aguardando o Direct3D iniciar...\n", pid);
    while (FindPid() == pid && !HasModule(pid, L"d3d9.dll")) Sleep(500);
    if (FindPid() != pid) continue;
    Sleep(4000);
    if (HasModule(pid, kDllName)) printf("Overlay ja estava carregado.\n");
    else if (Inject(pid, dll)) printf("Overlay carregado! Aperte F8 dentro do jogo.\n");
    else printf("Nao foi possivel carregar o overlay.\n");
    while (FindPid() == pid) Sleep(2000);
    printf("\nJogo fechado. Aguardando uma nova abertura...\n");
  }
}
