cmake_minimum_required(VERSION 3.21)
project(RE5Trainer C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Compile em 32 bits (-A Win32): o RE5 e um processo de 32 bits.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")   # sem precisar de redistribuiveis do VC++

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR}/out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/out)

include(FetchContent)
FetchContent_Declare(imgui   GIT_REPOSITORY https://github.com/ocornut/imgui.git          GIT_TAG v1.91.5 SOURCE_SUBDIR _none)
FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git    GIT_TAG v1.3.3  SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(imgui minhook)

add_library(RE5Overlay SHARED
  dllmain.cpp features.cpp ui.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
  ${minhook_SOURCE_DIR}/src/buffer.c
  ${minhook_SOURCE_DIR}/src/hook.c
  ${minhook_SOURCE_DIR}/src/trampoline.c
  ${minhook_SOURCE_DIR}/src/hde/hde32.c)
target_include_directories(RE5Overlay PRIVATE
  ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${minhook_SOURCE_DIR}/include)
target_compile_definitions(RE5Overlay PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
target_compile_options(RE5Overlay PRIVATE /utf-8 /MP)
target_link_libraries(RE5Overlay PRIVATE d3d9 dinput8 dxguid dwmapi)

add_executable(RE5Trainer injector.cpp)
target_compile_definitions(RE5Trainer PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
target_compile_options(RE5Trainer PRIVATE /utf-8)
