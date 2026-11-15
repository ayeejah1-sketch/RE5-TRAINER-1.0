#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdarg>

void Log(const char* fmt, ...);
extern std::string g_dir;   // pasta da DLL, com barra final
