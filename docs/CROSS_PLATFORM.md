# Cross-Platform Build Guide

> Instructions for building Synapse on Linux and Windows.

---

## Supported Platforms

| Platform      | OS APIs Used       | Status       |
|---------------|--------------------|--------------|
| Linux (X11)   | libX11, XTest      | ✅ Primary   |
| Linux (Wayland) | libwayland, wlr-protocols | 🔲 Planned v0.4 |
| Windows 10+   | Win32 API (user32) | ✅ Primary   |
| macOS         | —                  | ❌ Not planned |

---

## Prerequisites

### Linux
```bash
# Debian/Ubuntu
sudo apt install build-essential cmake libx11-dev libxtst-dev

# Arch Linux
sudo pacman -S base-devel cmake libx11 libxtst

# Fedora
sudo dnf install gcc-c++ cmake libX11-devel libXtst-devel
```

### Windows
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload  
  OR  
- [MSYS2](https://www.msys2.org/) with MinGW-w64 toolchain
- [CMake 3.10+](https://cmake.org/download/)

---

## Building from Source

### Step 1: Clone the Repository
```bash
git clone https://github.com/yourname/synapse.git
cd synapse
```

### Step 2: Configure with CMake
```bash
mkdir build
cd build
cmake ..
```

For a Release (optimized) build:
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Step 3: Compile
```bash
# Linux / macOS
make -j$(nproc)

# Windows (MSVC via CMake)
cmake --build . --config Release
```

### Step 4: Run
```bash
# Linux
./synapse run ../examples/hello.syn

# Windows
synapse.exe run ..\examples\hello.syn
```

---

## CMake Configuration

The `CMakeLists.txt` automatically detects the target platform:

```cmake
cmake_minimum_required(VERSION 3.10)
project(Synapse)
set(CMAKE_CXX_STANDARD 17)
include_directories(include)

file(GLOB_RECURSE CORE_SRC
    src/lexer/*.cpp
    src/parser/*.cpp
    src/semantic/*.cpp
    src/interpreter/*.cpp
    src/main.cpp
)

if (UNIX AND NOT APPLE)
    list(APPEND CORE_SRC src/platform/linux/x11_platform.cpp)
    set(PLATFORM_LIBS X11 Xtst)
elseif (WIN32)
    list(APPEND CORE_SRC src/platform/windows/win32_platform.cpp)
    set(PLATFORM_LIBS user32 gdi32)
endif()

add_executable(synapse ${CORE_SRC})
target_link_libraries(synapse ${PLATFORM_LIBS})
```

---

## OS-Specific Implementation Notes

### Linux (X11)
- **Mouse control:** Uses `XWarpPointer()` for movement and `XTestFakeButtonEvent()` for clicks
- **Keyboard:** Uses `XTestFakeKeyEvent()` for key simulation
- **Window management:** Uses `XFetchName()`, `XRaiseWindow()`, `XMoveWindow()`, `XResizeWindow()`
- **Requires:** `DISPLAY` environment variable to be set (standard in all X11 sessions)

#### Wayland Note
On pure Wayland sessions (no XWayland), X11 APIs will not work. Wayland support via `libwayland` and protocols such as `wlr-layer-shell` and `wlr-virtual-pointer` is planned for v0.4.

### Windows (Win32)
- **Mouse control:** Uses `SetCursorPos()` and `SendInput()` with `MOUSEEVENTF_*` flags
- **Keyboard:** Uses `SendInput()` with `INPUT` structures
- **Window management:** Uses `FindWindow()`, `SetForegroundWindow()`, `MoveWindow()`, `ShowWindow()`
- **UAC Note:** Scripts that interact with UAC-elevated windows may require the `synapse.exe` process to also run elevated

---

## CLI Usage

```
synapse <command> [options]

Commands:
  run <file.syn>        Execute a Synapse script
  check <file.syn>      Validate syntax and semantics without executing
  version               Print version information
  help                  Print help

Options:
  --verbose             Enable verbose output (shows AST and execution steps)
  --dry-run             Parse and validate but do not execute OS commands
```

### Examples
```bash
synapse run my_automation.syn
synapse check my_script.syn
synapse run script.syn --verbose
synapse run script.syn --dry-run
```
