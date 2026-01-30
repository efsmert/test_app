# Wii U Homebrew Development Agent Guide

This document serves as a comprehensive reference for AI agents assisting with Wii U homebrew development in the Aroma environment. It contains detailed technical information about the toolchain, APIs, and best practices.

---

## Table of Contents

1. [Platform Overview](#platform-overview)
2. [Hardware Architecture](#hardware-architecture)
3. [Development Environment Setup](#development-environment-setup)
4. [Build System and Project Structure](#build-system-and-project-structure)
5. [WUT Framework Architecture](#wut-framework-architecture)
6. [Aroma-Specific Development](#aroma-specific-development)
7. [Graphics Programming (GX2)](#graphics-programming-gx2)
8. [Audio Engineering (sndcore2/AX)](#audio-engineering-sndcore2ax)
9. [Input Handling](#input-handling)
10. [Networking](#networking)
11. [Debugging and Testing](#debugging-and-testing)
12. [Common Pitfalls and Solutions](#common-pitfalls-and-solutions)
13. [Resources and References](#resources-and-references)

---

## Platform Overview

The Wii U homebrew scene in 2025 centers on **Aroma**, a mature custom firmware environment that enables apps to launch directly from the home menu as native channels. Unlike the legacy Homebrew Launcher approach, Aroma provides:

- Persistent background modules
- A plugin system with hot-reloading
- The **.wuhb bundle format** that packages executables with metadata and icons

### Key Terminology

| Term | Definition |
|------|------------|
| **Aroma** | Modern CFW environment for Wii U that allows native channel launching |
| **WUT** | Wii U Toolchain - the SDK for creating RPX/RPL executables |
| **RPX** | Relocatable Portable eXecutable - the main application format |
| **RPL** | Dynamic library equivalent of RPX |
| **WUHB** | Wii U Homebrew Bundle - packaging format for Aroma apps |
| **CafeOS** | The Wii U's operating system |
| **GX2** | The Wii U's low-level graphics API |
| **Espresso** | The Wii U's tri-core PowerPC CPU |
| **Latte** | The Wii U's AMD-based GPU |

---

## Hardware Architecture

### The Espresso CPU

The Wii U's central processing unit is the **Espresso**, a tri-core PowerPC 750CL-based processor clocked at **1.24 GHz**.

**Critical: Big-Endian Architecture**

The processor uses **Big-Endian byte order**. In Big-Endian systems, the most significant byte (MSB) of a multi-byte data type is stored at the lowest memory address.

This has profound implications for:
- File I/O
- Network communication
- Binary data parsing

Standard file formats (BMP, WAV, ZIP) and network protocols often use Little-Endian. Developers must implement byte-swapping:

| Data Type | Byte Swap Required | Function |
|-----------|-------------------|----------|
| `uint16_t` | Yes | `__builtin_bswap16(x)` |
| `uint32_t` | Yes | `__builtin_bswap32(x)` |
| `float` | Yes (via reinterpret cast) | Manual bit-reversal or union-based swapping |
| `char` / `uint8_t` | No | Direct assignment |

### Memory Architecture

The Wii U features a split memory architecture with **2 GB of RAM**:

| Pool | Size | Purpose |
|------|------|---------|
| **MEM1** | 32 MB | High-speed pool for backward compatibility and system-critical functions |
| **MEM2** | 1 GB (usable) | Primary application memory (remaining 1 GB reserved for OS) |
| **eDRAM** | 32 MB | ~140 GB/s bandwidth, ideal for render targets |

### Cache Coherency (CRITICAL)

The PowerPC CPU and AMD GPU do **not** share a fully coherent cache. When the CPU writes data for the GPU:

1. Data initially resides in CPU's L2 cache
2. GPU reads directly from physical RAM
3. Without explicit cache management, GPU reads stale data

**Required cache management functions:**

```c
// Force data from CPU cache to RAM (mandatory before GPU reads)
DCFlushRange(void* addr, size_t size);

// Discard CPU cache, force reload from RAM (mandatory before CPU reads GPU-written data)
DCInvalidateRange(void* addr, size_t size);
```

### GPU Specifications (Latte)

- Based on AMD's R700/TeraScale 2 architecture
- **320 stream processors** at 550 MHz
- ~352 GFLOPS
- Custom Radeon-family chip (similar to PC Radeon R7xx)

---

## Development Environment Setup

### Package Installation

The development stack consists of:
- **devkitPro** - Cross-compiler infrastructure
- **WUT** - Wii U Toolchain (headers and libraries for CafeOS)
- **wut-tools** - Utilities for creating RPX and WUHB

**Installation command:**

```bash
# Sync package databases and install the Wii U development group
(sudo) (dkp-)pacman -Syu --needed wiiu-dev
```

This installs:
- `devkitPPC` - PowerPC cross-compiler (`powerpc-eabi-gcc`, `powerpc-eabi-g++`)
- `wut` - System interface library with headers for coreinit, gx2, vpad, nn, etc.
- `wut-tools` - `elf2rpl`, `wuhbtool`, and other utilities

### Directory Structure

```
/opt/devkitpro/
├── devkitPPC/           # PowerPC GCC toolchain
├── wut/                 # Headers and stub libraries
├── portlibs/wiiu/       # Port libraries (SDL2, freetype, libpng, etc.)
├── tools/bin/           # elf2rpl, wuhbtool, rplexportgen
└── examples/wiiu/       # Sample projects
```

### Environment Variables

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH=$PATH:/opt/devkitpro/tools/bin:/opt/devkitpro/devkitPPC/bin
```

### Platform-Specific Setup

**Windows:**
1. Download graphical installer from `github.com/devkitPro/installer/releases`
2. Select "Wii U Development"
3. Run pacman command in MSYS terminal

**Linux:**
1. Install devkitPro pacman wrapper via official install script
2. Run pacman command

**macOS:**
1. Use pkg installer
2. Reboot to set environment variables

### Port Libraries (wiiu-portlibs)

Common third-party libraries available:

| Library | Purpose |
|---------|---------|
| `wiiu-sdl2-libs` | Hardware abstraction layer |
| `freetype` | Font rendering |
| `libpng` / `libjpeg-turbo` | Image decoding |
| `zlib` | Compression |

Install with:
```bash
(dkp-)pacman -S wiiu-sdl2-libs
```

### Docker for CI/CD

For reproducible builds:
```dockerfile
FROM devkitpro/devkitppc
RUN dkp-pacman -Syu --noconfirm wiiu-dev wiiu-sdl2-libs
```

---

## Build System and Project Structure

### CMake Configuration

CMake is the recommended build system. Use the WUT-provided toolchain file at `/opt/devkitpro/cmake/WiiU.cmake`.

**Complete CMakeLists.txt example:**

```cmake
cmake_minimum_required(VERSION 3.13)
project(MyAromaApp C CXX)

# Import the wut package to expose Wii U specific macros
find_package(wut REQUIRED)

# Define the source files
set(SOURCE_FILES src/main.cpp src/graphics.cpp src/audio.cpp)

# Create the executable target (initially an ELF)
add_executable(my_app ${SOURCE_FILES})

# Link against system libraries provided by wut
target_link_libraries(my_app
    wut
    coreinit
    gx2
    vpad
    sndcore2
)

# Convert ELF to RPX
wut_create_rpx(my_app)

# Package into WUHB
wut_create_wuhb(my_app
    NAME "My Aroma Application"
    AUTHOR "Development Team"
    SHORTNAME "MyAromaApp"
    ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/meta/icon.png"
    TVSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/assets/meta/splash_tv.png"
    DRCSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/assets/meta/splash_drc.png"
    CONTENT "${CMAKE_CURRENT_SOURCE_DIR}/content"
)
```

**Build commands:**

```bash
mkdir build && cd build
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ../
make
```

### Alternative: Modern CMake with wut targets

```cmake
cmake_minimum_required(VERSION 3.7)
project(myapp CXX C)

include("${DEVKITPRO}/wut/share/wut.cmake" REQUIRED)

add_executable(myapp source/main.cpp)

target_link_libraries(myapp
    wut::coreinit
    wut::vpad
    wut::gx2
    wut::proc_ui
    wut::sysapp
)

wut_create_rpx(myapp)
wut_create_wuhb(myapp
    NAME "My Application"
    SHORTNAME "MyApp"
    AUTHOR "Developer"
    ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
)
```

### wuhbtool Arguments

| CMake Argument | wuhbtool Flag | Description | Constraint |
|----------------|---------------|-------------|------------|
| NAME | --name | Display title on Wii U Menu | UTF-8 String |
| AUTHOR | --author | Developer attribution | UTF-8 String |
| ICON | --icon | Menu Icon | 128x128 PNG |
| TVSPLASH | --tv-image | Boot splash for TV | 1280x720 PNG |
| DRCSPLASH | --drc-image | Boot splash for GamePad | 854x480 PNG |
| CONTENT | --content | Directory to virtualize | Valid Path |
| SHORTNAME | --short-name | Abbreviated title | < 64 chars |

### The CONTENT Directory

The CONTENT argument maps a local directory to `/vol/content` inside the running application. This allows using standard filesystem calls (`fopen`, `fread`) to access assets without hardcoding SD card paths.

---

## WUT Framework Architecture

WUT provides C/C++ headers and stub libraries that interface with CafeOS through dynamic linking.

### Core Libraries

| Library | Header | Purpose |
|---------|--------|---------|
| **coreinit** | `<coreinit/*.h>` | OS fundamentals: threads, memory heaps, filesystem, time, atomic operations |
| **gx2** | `<gx2/*.h>` | Graphics API for the Latte GPU |
| **vpad** | `<vpad/input.h>` | GamePad input (buttons, sticks, gyro, touch) |
| **padscore** | `<padscore/*.h>` | Wii Remote, Classic Controller, Pro Controller |
| **proc_ui** | `<proc_ui/procui.h>` | Application lifecycle (foreground/background transitions) |
| **sndcore2** | `<sndcore2/*.h>` | AX audio system |
| **nsysnet** | `<nsysnet/*.h>` | BSD-style sockets |
| **nn_ac/nn_act** | `<nn/*.h>` | Network auto-connect, account management |

### Memory Management

```c
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>

// Default heap (simplest)
void* ptr = MEMAllocFromDefaultHeap(size);
MEMFreeToDefaultHeap(ptr);

// Aligned allocation (required for GPU resources)
void* aligned = MEMAllocFromDefaultHeapEx(size, GX2_VERTEX_BUFFER_ALIGNMENT);
```

### libwhb Helper Library

WUT provides `libwhb` (Wii U Homebrew helper) for common tasks:

| Function | Purpose |
|----------|---------|
| `WHBProcInit()` | Initialize OS process management (required) |
| `WHBProcIsRunning()` | Main loop condition |
| `WHBProcShutdown()` | Clean shutdown |
| `WHBLogConsoleInit()` | Debug console setup |
| `WHBLogConsoleFree()` | Free debug console |
| `WHBGfxInit()` | Initialize graphics subsystem |

---

## Aroma-Specific Development

### Architectural Differences from Homebrew Launcher

| Aspect | Homebrew Launcher | Aroma |
|--------|-------------------|-------|
| Launch method | Load into HBL, select app | Direct launch from home menu |
| File format | .elf or .rpx | .rpx or **.wuhb only** |
| Background services | Not possible | Plugins run alongside games |
| Memory stability | Shared heaps, crash-prone | Separate heaps per plugin |
| Exploits | Apps can launch own exploits | **Must not launch exploits** |

### Aroma Ecosystem Components

- **Modules** - Persistent background libraries exporting functions
  - KernelModule - Privileged access
  - FunctionPatcherModule - OS hooking
  - CURLWrapperModule - HTTPS support
- **Plugins** - WUPS-based extensions with user configuration
- **homebrew_on_menu plugin** - Scans SD for .wuhb files and displays them as channels

### WUHB Bundle Creation

**Asset requirements:**
- **Icon**: 128×128 PNG or TGA
- **TV splash**: 1280×720 (shown during load)
- **GamePad splash**: 854×480
- **Content directory**: Up to 4 GB of bundled data

**Command-line packaging:**

```bash
wuhbtool myapp.rpx myapp.wuhb \
  --name="My Application" \
  --short-name="MyApp" \
  --author="Developer" \
  --icon=icon.png \
  --tv-image=splash_tv.png \
  --drc-image=splash_drc.png \
  --content=./content
```

**Deployment:** Place `.wuhb` in `sd:/wiiu/apps/`

### MANDATORY: ProcUI Implementation

**Every Aroma homebrew must implement the ProcUI lifecycle properly.** This handles foreground/background transitions when users press HOME.

```c
#include <proc_ui/procui.h>
#include <coreinit/foreground.h>

int main(int argc, char **argv) {
    ProcUIInit(&OSSavesDone_ReadyToRelease);
    
    while (ProcUIProcessMessages(TRUE) != PROCUI_STATUS_EXITING) {
        // Main loop: render, handle input
    }
    
    ProcUIShutdown();
    return 0;
}
```

**Alternative using WHB:**

```c
#include <whb/proc.h>

int main(int argc, char **argv) {
    WHBProcInit();
    
    while (WHBProcIsRunning()) {
        // Main loop
    }
    
    WHBProcShutdown();
    return 0;
}
```

**Failing to implement ProcUI correctly causes crashes or hangs when users access the HOME menu.**

---

## Graphics Programming (GX2)

### GX2 Design Philosophy

Unlike OpenGL, **GX2 has no state retention**. Key characteristics:

- Commands queue directly to the GPU
- State saved/restored only through explicit `GX2ContextState` objects
- Memory management is entirely manual
- Strict alignment requirements (typically 0x100 / 256 bytes)

### Initialization

**Simple approach with WHB:**

```c
#include <whb/proc.h>
#include <whb/gfx.h>

void initGraphics() {
    WHBProcInit();
    WHBGfxInit();  // Handles GX2Init, context, and scan buffers
}
```

**Manual scan buffer setup:**

```c
GX2TVRenderMode tvMode = GX2_TV_RENDER_MODE_WIDE_720P;
GX2SurfaceFormat format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;

uint32_t tvSize, unk;
GX2CalcTVSize(tvMode, format, GX2_BUFFERING_MODE_DOUBLE, &tvSize, &unk);
void* tvScanBuffer = MEMAllocFromDefaultHeapEx(tvSize, GX2_SCAN_BUFFER_ALIGNMENT);
GX2SetTVBuffer(tvScanBuffer, tvSize, tvMode, format, GX2_BUFFERING_MODE_DOUBLE);
GX2SetTVEnable(TRUE);
```

### OSScreen - Simple 2D Output

For basic text and 2D output:

```c
#include <coreinit/screen.h>

OSScreenInit();

while (WHBProcIsRunning()) {
    // Clear both screens
    OSScreenClearBufferEx(SCREEN_DRC, 0xFF000000);  // GamePad
    OSScreenClearBufferEx(SCREEN_TV, 0xFF000000);   // TV
    
    // Draw text
    OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Hello GamePad!");
    OSScreenPutFontEx(SCREEN_TV, 0, 0, "Hello TV!");
    
    // Flip buffers
    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
}
```

### Shader Management with CafeGLSL

CafeGLSL provides runtime GLSL compilation:

```c
#include "CafeGLSLCompiler.h"

const char* vertexSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vertexColor;

void main() {
    gl_Position = vec4(aPos, 1.0);
    vertexColor = aColor;
}
)";

GLSL_Init();
char infoLog[1024];
GX2VertexShader* vs = GLSL_CompileVertexShader(vertexSource, infoLog, 
                                                sizeof(infoLog), 
                                                GLSL_COMPILER_FLAG_NONE);
```

**Requirements:**
- `glslcompiler.rpl` must be present
- Only separable shaders with explicit binding locations supported

### Dual-Screen Rendering

The Wii U's signature feature - simultaneous TV (1280×720) and GamePad (854×480) output:

```c
void renderFrame() {
    // Render to TV
    GX2SetColorBuffer(&tvColorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0, 0, 1280, 720, 0, 1);
    GX2ClearColor(&tvColorBuffer, 0.0f, 0.0f, 0.2f, 1.0f);
    // Draw scene...
    GX2CopyColorBufferToScanBuffer(&tvColorBuffer, GX2_SCAN_TARGET_TV);
    
    // Render to GamePad (can show different content)
    GX2SetColorBuffer(&drcColorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0, 0, 854, 480, 0, 1);
    GX2ClearColor(&drcColorBuffer, 0.0f, 0.2f, 0.0f, 1.0f);
    // Draw scene...
    GX2CopyColorBufferToScanBuffer(&drcColorBuffer, GX2_SCAN_TARGET_DRC);
    
    GX2SwapScanBuffers();
    GX2Flush();
    GX2WaitForVsync();
}
```

### Standard Render Loop Pattern

**Command-Draw-Swap:**

1. `GX2WaitForVsync()` - Sync with display refresh
2. `GX2SetContextState()` - Set current context
3. `GX2SetColorBuffer()`, `GX2SetDepthBuffer()` - Bind targets
4. `GX2ClearColor()`, `GX2ClearDepthStencil()` - Clear buffers
5. `GX2DrawEx()` - Execute draw calls
6. `GX2CopyColorBufferToScanBuffer()` - Copy to scan buffer
7. `GX2SwapScanBuffers()` - Swap

### CRITICAL: Cache Invalidation

**Mandatory when CPU-written data must be visible to GPU:**

```c
// After writing vertex data
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vertexBuffer, size);

// After writing texture data
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, textureData, textureSize);

// After writing shader programs
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shaderProgram, shaderSize);
```

**Forgetting this causes rendering glitches or crashes.**

### Texture Handling

Textures must be:
- Aligned to 0x100 (256 bytes) or larger boundaries
- Typically **swizzled** (tiled) for optimal GPU cache locality

```c
// Use GX2Swizzle_ functions to convert linear bitmaps to tiled format
// GX2Surface structure defines dimensions, format, and tiling mode
```

### SDL2 for Simpler Graphics

```cmake
find_package(SDL2 REQUIRED)
target_link_libraries(my_app SDL2::SDL2)
```

SDL2 on Wii U:
- Maps SDL_Window to GX2 context
- Maps SDL_Joystick to VPAD/KPAD
- Supports hardware-accelerated 2D rendering
- Does NOT support full OpenGL contexts (use GX2 directly for 3D)

### Raylib Port

Available via wiiu-portlibs. Abstracts GX2 complexity and translates Raylib's immediate mode calls to GX2 commands.

---

## Audio Engineering (sndcore2/AX)

The AX API provides low-level access to the audio DSP with up to **96 concurrent voices** and hardware-accelerated mixing.

### Initialization

```c
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include <sndcore2/device.h>

AXInitParams params = {
    .renderer = AX_INIT_RENDERER_48KHZ,
    .pipeline = AX_INIT_PIPELINE_SINGLE
};
AXInitWithParams(&params);
```

### Voice Setup and Playback

```c
// Acquire a voice (priority 0-31, higher = more important)
AXVoice *voice = AXAcquireVoice(31, NULL, NULL);

// Configure audio buffer (16-bit PCM, BIG-ENDIAN!)
AXVoiceOffsets offsets = {
    .dataType = AX_VOICE_FORMAT_LPCM16,
    .loopingEnabled = AX_VOICE_LOOP_DISABLED,
    .endOffset = sampleCount - 1,
    .currentOffset = 0,
    .data = audioBuffer
};
AXSetVoiceOffsets(voice, &offsets);

// Set volume (0x8000 = max)
AXVoiceVeData ve = { .volume = 0x8000, .delta = 0 };
AXSetVoiceVe(voice, &ve);

// Route to TV and GamePad
AXVoiceDeviceMixData tvMix[6] = {0};
tvMix[0].bus[0].volume = 0x8000;  // Left
tvMix[1].bus[0].volume = 0x8000;  // Right
AXSetVoiceDeviceMix(voice, AX_DEVICE_TYPE_TV, 0, tvMix);

AXSetVoiceState(voice, AX_VOICE_STATE_PLAYING);
```

### CRITICAL: Audio Format Requirements

**Audio samples must be Big-Endian!**

When loading WAV files (Little-Endian), perform 16-bit byte swap on every sample:

```c
// Swap bytes for each 16-bit sample
for (int i = 0; i < sampleCount; i++) {
    samples[i] = __builtin_bswap16(samples[i]);
}
```

**Failure to do this results in high-amplitude noise.**

### Supported Formats

| Format | Description |
|--------|-------------|
| `AX_VOICE_FORMAT_LPCM16` | 16-bit signed PCM (standard uncompressed) |
| `AX_VOICE_FORMAT_LPCM8` | 8-bit signed PCM |
| `AX_VOICE_FORMAT_ADPCM` | Nintendo's DSP ADPCM (compressed) |

### Buffer Chaining for Streaming

For long audio tracks, use double-buffering:

1. While buffer A plays, CPU fills buffer B
2. Use `AXRegisterAppFrameCallback` for interrupt notification
3. Swap buffers when DSP finishes

**Always flush audio buffer with `DCFlushRange()` before playback.**

### SDL2_mixer Alternative

For simpler needs:
```bash
(dkp-)pacman -S wiiu-sdl2_mixer
```

Supports MP3, OGG, WAV with automatic format conversion.

---

## Input Handling

### GamePad Input (VPAD)

```c
#include <vpad/input.h>

VPADStatus status;
VPADReadError error;

VPADRead(VPAD_CHAN_0, &status, 1, &error);

if (error == VPAD_READ_SUCCESS) {
    // Buttons
    if (status.trigger & VPAD_BUTTON_A) { /* Just pressed */ }
    if (status.hold & VPAD_BUTTON_B)    { /* Currently held */ }
    if (status.release & VPAD_BUTTON_X) { /* Just released */ }
    
    // Analog sticks (-1.0 to 1.0)
    float leftX = status.leftStick.x;
    float leftY = status.leftStick.y;
    float rightX = status.rightStick.x;
    float rightY = status.rightStick.y;
    
    // Gyroscope
    float gyroX = status.gyro.x;
    float gyroY = status.gyro.y;
    float gyroZ = status.gyro.z;
    
    // Accelerometer
    float accX = status.acc.x;
    
    // Touch screen
    if (status.tpNormal.validity == VPAD_VALID) {
        VPADTouchData calibrated;
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &calibrated, &status.tpNormal);
        int touchX = calibrated.x;
        int touchY = calibrated.y;
    }
}
```

### Button Constants

| Button | Constant |
|--------|----------|
| A, B, X, Y | `VPAD_BUTTON_A`, `VPAD_BUTTON_B`, `VPAD_BUTTON_X`, `VPAD_BUTTON_Y` |
| D-Pad | `VPAD_BUTTON_UP`, `VPAD_BUTTON_DOWN`, `VPAD_BUTTON_LEFT`, `VPAD_BUTTON_RIGHT` |
| Shoulders | `VPAD_BUTTON_L`, `VPAD_BUTTON_R`, `VPAD_BUTTON_ZL`, `VPAD_BUTTON_ZR` |
| System | `VPAD_BUTTON_PLUS`, `VPAD_BUTTON_MINUS`, `VPAD_BUTTON_HOME` |
| Sticks | `VPAD_BUTTON_STICK_L`, `VPAD_BUTTON_STICK_R` |

### Touch Screen Resolution Note

GamePad touch resolution is 854×480. When mirroring content to TV (1080p/720p), scale touch coordinates relative to the render target.

### Wii Remote and Pro Controller (KPAD)

```c
#include <padscore/kpad.h>
#include <padscore/wpad.h>

KPADInit();
WPADEnableURCC(TRUE);  // Enable Pro Controller

KPADStatus kpadData;
int32_t error;

for (int chan = 0; chan < 4; chan++) {
    if (KPADReadEx((KPADChan)chan, &kpadData, 1, &error) > 0) {
        switch (kpadData.extensionType) {
            case WPAD_EXT_CORE:
                // Wii Remote only
                if (kpadData.trigger & WPAD_BUTTON_A) { /* A pressed */ }
                break;
                
            case WPAD_EXT_NUNCHUK:
                // Access nunchuk stick
                float nunchukX = kpadData.nunchuk.stick.x;
                float nunchukY = kpadData.nunchuk.stick.y;
                break;
                
            case WPAD_EXT_CLASSIC:
                // Classic Controller
                break;
                
            case WPAD_EXT_PRO_CONTROLLER:
                // Full dual-analog support
                float lx = kpadData.pro.leftStick.x;
                float ly = kpadData.pro.leftStick.y;
                float rx = kpadData.pro.rightStick.x;
                float ry = kpadData.pro.rightStick.y;
                break;
        }
    }
}
```

---

## Networking

### Socket Programming

```c
#include <nsysnet/_socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

socket_lib_init();

int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

struct sockaddr_in server = {0};
server.sin_family = AF_INET;
server.sin_port = htons(8080);
inet_aton("192.168.1.100", &server.sin_addr);

connect(sock, (struct sockaddr*)&server, sizeof(server));
send(sock, data, len, 0);
recv(sock, buffer, maxLen, 0);
close(sock);
```

### HTTP/HTTPS with libcurl

For Aroma apps, use **CURLWrapperModule** which bundles CA certificates:

```c
#include <curl/curl.h>

curl_global_init(CURL_GLOBAL_DEFAULT);
CURL *curl = curl_easy_init();

curl_easy_setopt(curl, CURLOPT_URL, "https://example.com/api");
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

CURLcode res = curl_easy_perform(curl);

curl_easy_cleanup(curl);
curl_global_cleanup();
```

**Requirements:**
- Link with `-lcurlwrapper`
- Ensure `CURLWrapperModule.wms` is in `sd:/wiiu/environments/aroma/modules/`

---

## Debugging and Testing

### Network Transfer with wiiload

The `wiiload_plugin` runs a background server for wireless deployment:

```bash
export WIILOAD=tcp:192.168.1.50  # Your Wii U's IP
wiiload myapp.wuhb
```

### Logging

Build with DEBUG flags:

```bash
make DEBUG=1          # Info + error logging
make DEBUG=VERBOSE    # Verbose output
```

Logs route to UDP port **4405**:

```bash
nc -u -l 4405  # Listen on PC
```

### GDB Debugging

Using `gdbstub_plugin`:

1. Load gdbstub_plugin via wiiload
2. App restarts waiting for connection on port 3000
3. Connect with GDB:

```gdb
set arch powerpc:750
set endian big
target remote tcp:192.168.1.50:3000
```

Features:
- Software breakpoints (up to 512)
- Hardware watchpoints
- Single-stepping
- Memory inspection

### Crash Analysis

Use address with your ELF file:

```bash
powerpc-eabi-addr2line -e myapp.elf 800084ac
```

Build with `-g -save-temps` for assembly listings.

### Testing Limitations

**Cemu limitations:**
- WUHB format NOT supported
- SD card virtualization incomplete
- Many homebrew features don't work

**Hardware testing is strongly recommended** for serious development.

---

## Common Pitfalls and Solutions

### 1. HOME Menu Crashes

**Cause:** Missing or incorrect ProcUI implementation

**Solution:**
```c
// ALWAYS use ProcUI loop
while (ProcUIProcessMessages(TRUE) != PROCUI_STATUS_EXITING) {
    // or
while (WHBProcIsRunning()) {
```

### 2. Rendering Glitches / GPU Corruption

**Cause:** Forgetting cache invalidation

**Solution:**
```c
// ALWAYS call after writing GPU data
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, buffer, size);
DCFlushRange(buffer, size);
```

### 3. Audio Static/Noise

**Cause:** Little-Endian audio data passed to Big-Endian DSP

**Solution:**
```c
// Byte-swap all 16-bit samples
for (int i = 0; i < count; i++) {
    samples[i] = __builtin_bswap16(samples[i]);
}
```

### 4. App Exits Immediately

**Cause:** Returning from main() without event loop

**Solution:** Implement proper main loop with `WHBProcIsRunning()` or `ProcUIProcessMessages()`

### 5. Memory Alignment Crashes

**Cause:** GPU resources not properly aligned

**Solution:**
```c
// Use aligned allocation for GPU resources
void* buffer = MEMAllocFromDefaultHeapEx(size, GX2_VERTEX_BUFFER_ALIGNMENT);
```

### 6. Network Data Corruption

**Cause:** Not handling endianness in network protocols

**Solution:**
```c
// Use network byte order functions
uint32_t netValue = htonl(hostValue);
uint32_t hostValue = ntohl(netValue);
```

### 7. Texture Loading Failures

**Cause:** Linear textures without swizzling

**Solution:** Use `GX2Swizzle_` functions or set appropriate tiling mode in `GX2Surface`

---

## Resources and References

### Official Documentation

| Resource | URL |
|----------|-----|
| WUT Documentation | wut.devkitpro.org |
| WiiUBrew Wiki | wiiubrew.org |
| devkitPro | devkitpro.org |
| Wii U Hacks Guide | wiiu.hacks.guide |

### GitHub Organizations

| Organization | Focus |
|--------------|-------|
| **wiiu-env** | Aroma, plugins, modules ecosystem |
| **devkitPro** | WUT, toolchain, core libraries |

### Community

| Platform | Link |
|----------|------|
| Discord: ForTheUsers (4TU) | discord.com/invite/F2PKpEj |

### Example Projects to Study

| Project | Demonstrates |
|---------|--------------|
| WUT samples | All major APIs |
| ftpiiu_plugin | Background plugin, networking |
| RetroArch Wii U | Complex GX2, input, audio |
| Dumpling | Filesystem, UI, networking |

---

## Complete Example: Minimal Aroma Application

```c
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <coreinit/screen.h>
#include <vpad/input.h>

int main(int argc, char **argv) {
    // Initialize systems
    WHBProcInit();
    WHBLogConsoleInit();
    OSScreenInit();
    VPADInit();
    
    // Allocate screen buffers
    uint32_t tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    void* tvBuffer = MEMAllocFromDefaultHeapEx(tvBufferSize, 0x100);
    void* drcBuffer = MEMAllocFromDefaultHeapEx(drcBufferSize, 0x100);
    OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);
    
    // Main loop
    while (WHBProcIsRunning()) {
        // Read input
        VPADStatus vpadStatus;
        VPADReadError vpadError;
        VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);
        
        // Check for exit
        if (vpadStatus.trigger & VPAD_BUTTON_HOME) {
            break;
        }
        
        // Clear screens
        OSScreenClearBufferEx(SCREEN_TV, 0x000000FF);
        OSScreenClearBufferEx(SCREEN_DRC, 0x000000FF);
        
        // Draw text
        OSScreenPutFontEx(SCREEN_TV, 0, 0, "Hello Wii U! (TV)");
        OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Hello Wii U! (GamePad)");
        OSScreenPutFontEx(SCREEN_DRC, 0, 2, "Press HOME to exit");
        
        // Flip buffers
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }
    
    // Cleanup
    MEMFreeToDefaultHeap(tvBuffer);
    MEMFreeToDefaultHeap(drcBuffer);
    VPADShutdown();
    OSScreenShutdown();
    WHBLogConsoleFree();
    WHBProcShutdown();
    
    return 0;
}
```

---

## Summary Workflow

1. **Setup**: Install devkitPro + wiiu-dev
2. **Code**: Use WUT libraries (coreinit, gx2, vpad, sndcore2)
3. **Initialize**: `WHBProcInit()`, `VPADInit()`, `OSScreenInit()`
4. **Main Loop**: `while (WHBProcIsRunning()) { ... }`
5. **Render**: Clear, draw, flip for both screens
6. **Cleanup**: Free resources, `WHBProcShutdown()`
7. **Build**: CMake + wut_create_rpx + wut_create_wuhb
8. **Deploy**: Copy .wuhb to `sd:/wiiu/apps/`
9. **Launch**: Boot Aroma, select from home menu

---

## CRITICAL: Project-Specific Context

**Before beginning any work on this codebase, the agent MUST read the `SUMMARY.md` file located in the project root.**

This file contains:
- Summaries of work completed by previous agents
- Project-specific decisions and architectural choices
- Current state of implementation
- Known issues and their workarounds
- Task history and progress tracking

The `SUMMARY.md` file provides essential project context that supplements the general Wii U development knowledge in this document. Failing to read it may result in:
- Duplicating work already completed
- Contradicting established project conventions
- Missing important context about why certain decisions were made
- Breaking existing functionality

**Always start by reading `SUMMARY.md` to understand the current project state before making any changes.**

**Use the following command to compile the project:**

```set -euo pipefail
python3 smb_wiiu/tools/godot_levels_to_cpp.py
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"
make -C smb_wiiu
make -C smb_wiiu wuhb
make -C smb_wiiu cemu-sync
ls -la smb_wiiu/smb_wiiu.wuhb smb_wiiu/smb_wiiu_cemu/code/smb_wiiu.rpx | sed -n '1,5p'
```

---

*This document compiled from comprehensive Wii U homebrew development research for use by AI coding agents.*

## Code Documentation for Context Continuity

Agents working on this codebase will inevitably hit context limits and need to resume work in fresh sessions. Write code that helps future agents (including yourself after a reset) understand the codebase quickly.

### Required Documentation

**Function/Class Docstrings (MANDATORY)**
Every non-trivial function must have a brief docstring explaining:
- What it does (purpose, not implementation)
- Why it exists if non-obvious
- Any gotchas or Wii U-specific considerations

```cpp
// Renders a single tile to the GX2 color buffer.
// Uses CPU-side vertex buffer that MUST be cache-flushed before draw.
// Called per-tile because batching caused z-fighting on hardware.
void renderTile(int tileId, float x, float y);
```

**Non-Obvious Logic**
Comment any code where the "why" isn't immediately clear:
```cpp
// Byte-swap required: Wii U is big-endian, WAV files are little-endian
samples[i] = __builtin_bswap16(samples[i]);
```

**Wii U-Specific Workarounds**
Always document platform quirks:
```cpp
// GX2 requires 256-byte alignment for vertex buffers - smaller alignments
// cause silent corruption on hardware (works fine in Cemu)
void* vbo = MEMAllocFromDefaultHeapEx(size, 0x100);
```

### What NOT to Comment

- Trivial operations (`i++`, simple assignments)
- Code that's self-explanatory from good naming
- Restating what the code does without adding "why"

### SUMMARY.md is Your Primary Context File

The `SUMMARY.md` file is more valuable for context recovery than inline comments. When you complete significant work:

1. Update your agent section in SUMMARY.md with:
   - What you implemented/changed
   - Key decisions and why you made them
   - Gotchas you discovered
   - What's left to do

2. Keep inline comments focused on local "why" questions
3. Keep SUMMARY.md focused on architectural/project-level context

**The goal: A future agent should be able to read SUMMARY.md + function docstrings and understand the codebase without reading every line of code.**

**Self improvement**
Continuously improve agent workflows.

When a repeated correction or better approach to something is discovered, or you make any significant changes to the project you MUST codify your new found knowledge and learnings by modifying your section of SUMMARY.md.

You can modify SUMMARY.md without prior aproval as long as your edits continue at the end of the file.

Be very detailed when you codify your instructions.

If you utlise any of your codified instructions in future coding sessions call that out and let the user know that you peformed the action because of that specific rule in this file.

We want to save time when you learn things so that we are not constantly searching for the same information over and over again.
