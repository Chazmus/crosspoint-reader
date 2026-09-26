# CrossPoint Reader Development Guide (ESP32-S3 Fork)

Project: Open-source e-reader firmware targeting ESP32-S3 devices (e.g., Xteink X4 Pro, Seeed reTerminal Sticky, M5PaperMono).
Mission: Provide a high-performance reading and application experience taking full advantage of the dual-core ESP32-S3, PSRAM, and touch screen capabilities.

> **Note on Fork Target**: This fork specifically targets **ESP32-S3** hardware. Legacy ESP32-C3-only constraints (no PSRAM, extreme DRAM starvation) are superseded by the S3 architecture (e.g., 8MB Octal PSRAM on X4 Pro), enabling modular apps, games, and richer features while maintaining rock-solid stability.

## AI Agent Identity and Cognitive Rules

* Role: Senior Embedded Systems Engineer (ESP-IDF/Arduino-ESP32 specialized).
* Target Architecture: ESP32-S3 (dual-core Xtensa LX7 @ 240MHz, PSRAM-enabled).
* Stability & Memory Discipline: Even with 8MB PSRAM on the S3, leak-free memory management, stack safety, and deterministic resource release are non-negotiable.
* Evidence-Based Reasoning: Before proposing a change, you MUST cite the specific file path and line numbers that justify the modification.
* Anti-Hallucination: Do not assume the existence of libraries or ESP-IDF functions. Check the freeink-sdk source or the FreeInk SDK docs (https://freeink.org/llms.txt for an LLM-readable index) first.
* No Unfounded Claims: Do not claim performance gains or memory savings without explaining the technical mechanism.
* Resource Justification: Explain heap allocations and memory placement (PSRAM vs internal DRAM vs Flash).
* Verification: After suggesting a fix, instruct the user on how to verify it (e.g., monitoring heap via Serial or checking a specific cache file).

---

## Development Environment Awareness

**CRITICAL**: Detect the host platform at session start to choose appropriate tools and commands.

### Platform Detection

```bash
# Detect platform (run once per session)
uname -s
# Returns: MINGW64_NT-* (Windows Git Bash), Linux, Darwin (macOS)
```

**Detection Required**: Run `uname -s` at session start to determine platform

### Platform-Specific Behaviors

- **Windows (Git Bash)**: Unix commands, `C:\` paths in Windows but `/` in bash, limited glob (use `find`+`xargs`)
- **Linux/WSL**: Full bash, Unix paths, native glob support

**Cross-Platform Code Formatting**:

```bash
./bin/clang-format-fix -g
```

Never invoke or probe `clang-format` directly. The repository wrapper is the only sanctioned entry point.

---

## Platform and Hardware Constraints

### Hardware Specs (ESP32-S3 Focus)

* MCU: ESP32-S3 (dual-core Xtensa LX7 @ 240MHz)
* RAM: 512KB internal SRAM + 8MB Octal PSRAM (`dio_opi` on Xteink X4 Pro)
  * Rich memory headroom for apps, caches, and networking.
* Flash: 16MB (Instruction storage and static data)
* Display: 800x480 E-Ink (SSD1677 on X4 Pro)
  * Framebuffer: 48,000 bytes (800 × 480 ÷ 8)
* Touch & Input: Capacitive touch (Goodix GT911 on X4 Pro) + physical buttons / capacitive home button
* Storage: SD Card / SDMMC (Used for books, apps, and aggressive caching)

### The Resource Protocol

1. Stack Safety: Limit local function variables to < 256 bytes. The ESP32-C3 default stack is small; use std::unique_ptr or static pools for larger buffers.
2. Heap Fragmentation: Avoid repeated new/delete in loops. Allocate buffers once during onEnter() and reuse them.
3. Flash Persistence: Large constant data (UI strings, lookup tables) MUST be marked static const to stay in Flash (Instruction Bus), freeing DRAM.
4. String Policy: Prohibit std::string and Arduino String in hot paths. Use std::string_view for read-only access and snprintf with fixed char[] buffers for construction.
5. UI Strings: All user-facing text must use the `tr()` macro (e.g., `tr(STR_LOADING)`) for i18n support. Never hardcode UI strings directly. For the avoidance of doubt, logging messages (LOG_DBG/LOG_ERR) can be hardcoded, but user-facing text must use `tr()`.
6. `constexpr` First: Compile-time constants and lookup tables must be `constexpr`, not just `static const`. This moves computation to compile time, enables dead-branch elimination, and guarantees flash placement. Use `static constexpr` for class-level constants.
7. `std::vector` Pre-allocation: Always call `.reserve(N)` before any `push_back()` loop. Each growth event allocates a new block (2×), copies all elements, then frees the old one — three heap operations that fragment DRAM. When the final size is unknown, estimate conservatively.
8. SD Persistence Throttling: Settings, state, credentials, and other `PersistableStore` JSON files live on SD under `/.crosspoint/` through `HalStorage`; SPIFFS is not mounted. Guard redundant writes and debounce progress saves to avoid serialization, SD I/O, and `storageMutex` cost.
9. `new` is not nothrow on ESP32: With `-fno-exceptions`, bare `new` that fails calls `abort()` — it does NOT return `nullptr`. Always use `new (std::nothrow)` and null-check the result, or use `makeUniqueNoThrow<T>()` from `lib/Memory/Memory.h`. Never write bare `new` for any fallible allocation.

---

## Project Architecture

### Build System: PlatformIO

**PlatformIO is BOTH a VS Code extension AND a CLI tool**:

1. **VS Code Extension** (Recommended):
   
   * Extension ID: `platformio.platformio-ide` (see `.vscode/extensions.json`)
   
   * Provides: Toolbar buttons, IntelliSense, integrated build/upload/monitor
   
   * Configuration: `.vscode/c_cpp_properties.json`, `.vscode/tasks.json`
   
   * Usage: Click Build (✓), Upload (→), or Monitor (🔌) buttons

2. **CLI Tool** (`pio` command):
   
   * **Installation**: Python package (typically `pip install platformio`)
   
   * **Windows Location**: `C:\Users\<user>\AppData\Local\Programs\Python\Python3xx\Scripts\pio.exe`
   
   * **Verify**: `which pio` (Git Bash) or `where.exe pio` (cmd)
   
   * **Usage**: `pio run`, `pio run -t upload`, etc.

**Configuration Files**:

* `platformio.ini`: Main build configuration (committed to git)
* `platformio.local.ini`: Local overrides (gitignored, create if needed)
* `partitions.csv`: ESP32 flash partition layout

### Build Environment

* **Standard**: C++20 (`-std=c++2a`). No Exceptions, No RTTI.
* **Logging**: ALWAYS use `LOG_INF`, `LOG_DBG`, or `LOG_ERR` from `Logging.h`. Raw Serial output is deprecated.
* **Environments** (in `platformio.ini`):
  * **`x4pro` (PRIMARY TARGET)**: Development/default build for Xteink X4 Pro (`pio run -e x4pro`). ESP32-S3 (`esp32-s3-devkitc1-n16r8`), 8MB Octal PSRAM (`dio_opi`), 16MB Flash, SSD1677 800x480 + GT911 touchscreen, native 1-bit SDMMC, USB CDC/MSC, `LOG_LEVEL=2`. **Always compile and test against `x4pro` in this repository.**
  * `x4c`: Xteink X4 Classic buttons-only variant (no touch, no frontlight).
  * `sticky`: Seeed reTerminal Sticky (ESP32-S3, 8MB PSRAM, SSD1677 + GT911).
  * Legacy C3 profiles: `default` (legacy C3 dev), `gh_release` (legacy C3 release), `slim`.

### Critical Build Flags

These flags in `platformio.ini` fundamentally affect firmware behavior:

```cpp
-DEINK_DISPLAY_SINGLE_BUFFER_MODE=1  // Single framebuffer (saves 48KB RAM!)
-DARDUINO_USB_MODE=1                 // Enable USB CDC
-DARDUINO_USB_CDC_ON_BOOT=1          // Serial available immediately at boot
-DXML_CONTEXT_BYTES=1024             // XML parser memory limit (EPUB parsing)
-DUSE_UTF8_LONG_NAMES=1              // SD card long filename support
-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1   // Avoid zlib name conflicts
-DXML_GE=0                           // Disable XML general entities (security)
-DDESTRUCTOR_CLOSES_FILE=1           // FsFile destructor auto-closes (SdFat)
```

**DESTRUCTOR_CLOSES_FILE implications**:

- SdFat's `FsBaseFile` destructor calls `close()` automatically when the object goes out of scope
- **Do NOT add explicit `file.close()` calls** for local `FsFile` variables — the destructor handles it
- Explicit `close()` is still required in these cases:
  
  1. **Close before delete**: Must close before `Storage.remove()` on the same path
  
  2. **Close before reopen**: Must close before reopening the same `FsFile` variable (e.g., write then reopen for read, or rewrite the same path)
  
  3. **Member variables**: `FsFile` members persist beyond any single function scope, so close at the intended release point (e.g., in `onExit()`)

**SINGLE_BUFFER_MODE implications**:

- Only ONE framebuffer exists (not double-buffered)
- Grayscale rendering requires temporary buffer allocation (`renderer.storeBwBuffer()`)
- Must call `renderer.restoreBwBuffer()` to free temporary buffers
- See [lib/GfxRenderer/GfxRenderer.cpp:439-440](lib/GfxRenderer/GfxRenderer.cpp) for malloc usage

### Directory Structure

* lib/: Internal libraries (Epub engine, GfxRenderer, UITheme, I18n)
  * lib/hal/: Hardware Abstraction Layer (HalDisplay, HalGPIO, HalStorage)
  * lib/I18n/: Internationalization (translations in `translations/*.yaml`, generated string tables)
* src/activities/: UI logic using the Activity Lifecycle (onEnter, loop, onExit)
* freeink-sdk/: Low-level SDK (EInkDisplay, InputManager, BatteryMonitor, SDCardManager)
* .crosspoint/: SD-based binary cache for EPUB metadata and pre-rendered layout sections

### Hardware Abstraction Layer (HAL)

**CRITICAL**: Always use HAL classes, NOT SDK classes directly.

| HAL Class    | Wraps SDK Class | Purpose               | Singleton Macro |
| ------------ | --------------- | --------------------- | --------------- |
| `HalDisplay` | `EInkDisplay`   | E-ink display control | *(none)*        |
| `HalGPIO`    | `InputManager`  | Button input handling | *(none)*        |
| `HalStorage` | `SDCardManager` | SD card file I/O      | `Storage`       |

**Location**: [lib/hal/](lib/hal/)

**Why HAL?**

- Provides consistent error logging per module
- Abstracts SDK implementation details
- Centralizes resource management

**Example - HalStorage**:

```cpp
#include <HalStorage.h>

// Use Storage singleton (defined via macro)
HalFile file;
if (Storage.openFileForRead("MODULE", "/path/to/file.bin", file)) {
  // Read from file
  // No file.close() needed — DESTRUCTOR_CLOSES_FILE=1 handles it at scope exit
}
```

**Usage**: Use `HalFile` (the mutex-wrapping handle), NOT raw SdFat `FsFile` or Arduino `File`. Do NOT add `file.close()` for local variables (see DESTRUCTOR_CLOSES_FILE above).

**SdFat is not thread-safe; all SD access MUST go through HalStorage**:

- SdFat's `SdSpiCard` tracks SPI bus state with an unsynchronized `m_spiActive` bool. Two tasks calling SdFat concurrently can confuse that state machine and end with one task calling `SPIClass::endTransaction()` against a paramLock the *other* task is holding. That trips FreeRTOS's `xTaskPriorityDisinherit` assert (`tasks.c:5156, pxTCB == pxCurrentTCBs[0]`) and panics the system. See SdFat issue #518.
- `HalStorage` serializes everything via `storageMutex`. Downstream code uses `HalFile` (declared in `<HalStorage.h>`); every method call (read, write, seek, close) takes the mutex. `HalFile`'s destructor also takes the mutex before letting the underlying SdFat `FsFile` close.
- **Never** call into `SdFat` / `SdSpiCard` / `FsBaseFile` / `SDCardManager` / raw `FsFile` directly — that bypasses the mutex.

---

## Coding Standards

### Naming Conventions

* Classes: PascalCase (e.g., EpubReaderActivity)
* Methods/Variables: camelCase (e.g., renderPage())
* Constants: UPPER_SNAKE_CASE (e.g., MAX_BUFFER_SIZE)
* Private Members: memberVariable (no prefix)
* File Names: Match Class names (e.g., EpubReaderActivity.cpp)

### Header Guards

* Use #pragma once for all header files.

### Comment Style

* Keep comments short and write them for the merged state, as if the code had always worked this way.
* Remove before/after narration, investigation measurements, and rationale that belongs in the commit message.
* Keep only non-obvious mechanism, field/parameter meaning, or the reason a special case exists.

### Memory Safety and RAII

* Smart Pointers: Prefer std::unique_ptr. 
* RAII: Use destructors for cleanup. Call `vTaskDelete()` explicitly for deterministic task release. Do NOT call `file.close()` on local `FsFile` variables — `DESTRUCTOR_CLOSES_FILE=1` handles it at scope exit (see Critical Build Flags).

### ESP32-C3 Platform Pitfalls

#### `std::string_view` and Null Termination

`string_view` is *not* null-terminated. Passing `.data()` to any C-style API (`drawText`, `snprintf`, `strcmp`, SdFat file paths) is undefined behaviour when the view is a substring or a view of a non-null-terminated buffer.

**Rule**: `string_view` is safe only when passing to C++ APIs that accept `string_view`. For any C API boundary, convert explicitly:

```cpp
// WRONG - undefined behaviour if view is a substring:
renderer.drawText(font, x, y, myView.data(), true);

// CORRECT - guaranteed null-terminated:
renderer.drawText(font, x, y, std::string(myView).c_str(), true);

// CORRECT - for short strings, use a stack buffer:
char buf[64];
snprintf(buf, sizeof(buf), "%.*s", (int)myView.size(), myView.data());
```

#### `IRAM_ATTR` and Flash Cache Safety

All code runs from flash via the instruction cache. During internal-flash operations such as OTA writes or NVS updates, the cache is briefly suspended. Any code that can execute during this window — ISRs in particular — must reside in IRAM or it will crash silently.

```cpp
// ISR handler: must be in IRAM
void IRAM_ATTR gpioISR() { ... }

// Data accessed from IRAM_ATTR code: must be in DRAM, never a flash const
static DRAM_ATTR uint32_t isrEventFlags = 0;
```

**Rules**:

- All ISR handlers: `IRAM_ATTR`
- Data read by `IRAM_ATTR` code: `DRAM_ATTR` (a flash-resident `static const` will fault)
- Normal task code does **not** need `IRAM_ATTR`

#### ISR vs Task Shared State

`xSemaphoreTake()` (mutex) **cannot** be called from ISR context — it will crash. Use the correct primitive for each communication direction:

| Direction                       | Correct primitive                                  |
| ------------------------------- | -------------------------------------------------- |
| ISR → task (data)               | `xQueueSendFromISR()` + `portYIELD_FROM_ISR()`     |
| ISR → task (signal)             | `xSemaphoreGiveFromISR()` + `portYIELD_FROM_ISR()` |
| Task → task                     | `xSemaphoreTake()` / mutex                         |
| Simple flag (single writer ISR) | `volatile bool` + `portENTER_CRITICAL_ISR()`       |

#### RISC-V Alignment

ESP32-C3 faults on unaligned multi-byte loads. Never cast a `uint8_t*` buffer to a wider pointer type and dereference it directly. Use `memcpy` for any unaligned read:

```cpp
// WRONG — faults if buf is not 4-byte aligned:
uint32_t val = *reinterpret_cast<const uint32_t*>(buf);

// CORRECT:
uint32_t val;
memcpy(&val, buf, sizeof(val));
```

This applies to all cache deserialization code and any raw buffer-to-struct casting. `__attribute__((packed))` structs have the same hazard when accessed via member reference.

#### Template and `std::function` Bloat

Each template instantiation generates a separate binary copy. `std::function<void()>` adds ~2–4 KB per unique signature and heap-allocates its closure. Avoid both in library code and any path called from the render loop:

```cpp
// Avoid — heap-allocating, large binary footprint:
std::function<void()> callback;

// Prefer — zero overhead:
void (*callback)() = nullptr;

// For member function + context (common activity callback pattern):
struct Callback { void* ctx; void (*fn)(void*); };
```

When a template is necessary, limit instantiations: use explicit template instantiation in a `.cpp` file to prevent the compiler from generating duplicates across translation units.

---

### Error Handling Philosophy

**Source**: [src/main.cpp:132-143](src/main.cpp), [lib/GfxRenderer/GfxRenderer.cpp:10](lib/GfxRenderer/GfxRenderer.cpp)

**Pattern Hierarchy**:

1. **LOG_ERR + return false** (90%): `LOG_ERR("MOD", "Failed: %s", reason); return false;`
2. **LOG_ERR + fallback**: `LOG_ERR("MOD", "Unavailable"); useDefault();`
3. **assert(false)**: Only for fatal "impossible" states (framebuffer missing)
4. **ESP.restart()**: Only for recovery (OTA complete)

**Rules**: NO exceptions, NO abort(), ALWAYS log before error return

### Heap Buffer Allocation

**Prefer `makeUniqueNoThrow` over `malloc`.** Both are nothrow (return `nullptr` on OOM rather than calling `abort()`), but `malloc` requires a manual `free` on every return path — a common source of leaks. `makeUniqueNoThrow<uint8_t[]>(size)` from `lib/Memory/Memory.h` frees automatically when it goes out of scope.

**Preferred pattern**:

```cpp
#include <Memory.h>

auto buffer = makeUniqueNoThrow<uint8_t[]>(bufferSize);
if (!buffer) {
  LOG_ERR("MODULE", "OOM: %d bytes", bufferSize);
  return false;
}

processData(buffer.get(), bufferSize);
// freed automatically — no manual free needed, no leak on early return
```

**`malloc` or `new (std::nothrow)` are still acceptable** when the buffer must be passed to a C API that takes ownership and frees it itself (e.g., certain SDK callbacks). In that case follow the manual pattern:

```cpp
auto* buffer = static_cast<uint8_t*>(malloc(bufferSize));  // or new (std::nothrow) uint8_t[bufferSize]
if (!buffer) {
  LOG_ERR("MODULE", "OOM: %d bytes", bufferSize);
  return false;
}
sdkApiThatTakesOwnership(buffer, bufferSize);  // SDK calls free() / delete[]
```

**Rules**:

- **Prefer `makeUniqueNoThrow`** — automatic cleanup eliminates leak risk on error paths
- **ALWAYS check for nullptr** after any allocation and `LOG_ERR` before returning false
- **Raw allocation only** when a C API takes ownership; document why in a comment

**Examples in codebase**:

- Memory utilities: [Memory.h](lib/Memory/Memory.h) (`makeUniqueNoThrow`)
- Cover image buffers: [HomeActivity.cpp:166](src/activities/home/HomeActivity.cpp)
- Bitmap rendering: [GfxRenderer.cpp:439-440](lib/GfxRenderer/GfxRenderer.cpp)

### Heap Allocation with `new`: Always Use `makeUniqueNoThrow`

**CRITICAL**: With `-fno-exceptions`, bare `new` on OOM calls `abort()` — it does NOT return `nullptr`. Always use `makeUniqueNoThrow` from `lib/Memory/Memory.h`, which wraps `new (std::nothrow)` and returns a `std::unique_ptr` that is null on OOM and automatically frees on scope exit.

**Preferred pattern**:

```cpp
#include <Memory.h>

auto obj = makeUniqueNoThrow<MyClass>(args);
if (!obj) { LOG_ERR("MOD", "OOM: MyClass"); return false; }

auto buf = makeUniqueNoThrow<uint8_t[]>(size);
if (!buf) { LOG_ERR("MOD", "OOM: %d bytes", size); return false; }

// Pass to C APIs via .get(); unique_ptr frees automatically on return
someApi(buf.get(), size);
```

**`new (std::nothrow)` directly is acceptable** when the object must be passed to a C API that takes ownership and calls `delete` itself:

```cpp
auto* obj = new (std::nothrow) MyClass(args);
if (!obj) { LOG_ERR("MOD", "OOM: MyClass"); return false; }
sdkApiThatTakesOwnership(obj);  // SDK calls delete
```

**Rules**:

- **Prefer `makeUniqueNoThrow`** — automatic cleanup eliminates leak risk on error paths
- **NEVER use bare `new`** — always `makeUniqueNoThrow` or `new (std::nothrow)`
- **ALWAYS `LOG_ERR` before returning false** on OOM
- **Use `.get()`** to pass the raw pointer to C-style APIs; ownership stays with the `unique_ptr`
- **`new (std::nothrow)` directly only** when a C API takes ownership; document why in a comment

**Examples in codebase**:

- Memory utilities: [Memory.h](lib/Memory/Memory.h) (`makeUniqueNoThrow`)

---

## UI and Orientation Guidelines

### Orientation-Aware Logic

* No Hardcoding: Never assume 800 or 480. Use renderer.getScreenWidth() and renderer.getScreenHeight().
* Viewable Area: Use renderer.getOrientedViewableTRBL() to stay within physical bezel margins.

### Logical Button Mapping

**Source**: [src/MappedInputManager.cpp:20-55](src/MappedInputManager.cpp)

Constraint: Physical button positions are fixed on hardware, but their logical functions change based on user settings and screen orientation.

**Button Categories**:

1. **Physical Fixed** (Up/Down side buttons):
   
   - `Button::Up` → Always `HalGPIO::BTN_UP`
   
   - `Button::Down` → Always `HalGPIO::BTN_DOWN`

2. **User Remappable** (Front buttons):
   
   - `Button::Back` → Maps to `SETTINGS.frontButtonBack` (hardware index)
   
   - `Button::Confirm` → Maps to `SETTINGS.frontButtonConfirm`
   
   - `Button::Left` → Maps to `SETTINGS.frontButtonLeft`
   
   - `Button::Right` → Maps to `SETTINGS.frontButtonRight`

3. **Reader-Specific** (Page navigation with optional swap):
   
   - `Button::PageBack` → Uses side button (swappable via `SETTINGS.sideButtonLayout`)
   
   - `Button::PageForward` → Uses side button (swappable)

**Implementation**:

- Activities use **logical buttons** (e.g., `Button::Confirm`)
- `MappedInputManager` translates to **physical hardware buttons**
- User can remap front buttons in settings
- Orientation changes handled separately by renderer coordinate transforms

**Rule**: Always use `MappedInputManager::Button::*` enums, never raw `HalGPIO::BTN_*` indices (except in ButtonRemapActivity).

### UITheme (The GUI Macro)

* Rule: All UI rendering must go through the GUI macro (UITheme). 
* Do not hardcode fonts, colors, or positioning. This ensures orientation-aware layout consistency.

---

## Common Patterns

### Singleton Access

**Available Singletons**:

```cpp
#define SETTINGS CrossPointSettings::getInstance()  // User settings
#define APP_STATE CrossPointState::getInstance()    // Runtime state
#define GUI UITheme::getInstance()                   // Current theme
#define Storage HalStorage::getInstance()            // SD card I/O
#define I18N I18n::getInstance()                     // Internationalization
```

### Activity Lifecycle and Memory Management

**Source**: [src/main.cpp:132-143](src/main.cpp)

**CRITICAL**: Activities are **heap-allocated** and **deleted on exit**.

```cpp
// main.cpp navigation pattern
void exitActivity() {
  if (currentActivity) {
    currentActivity->onExit();
    delete currentActivity;  // Activity deleted here!
    currentActivity = nullptr;
  }
}

void enterNewActivity(Activity* activity) {
  currentActivity = activity;  // Heap-allocated activity
  currentActivity->onEnter();
}
```

**Memory Implications**:

- Activity navigation = `delete` old activity + `new` create next activity
- Any memory allocated in `onEnter()` MUST be freed in `onExit()`
- FreeRTOS tasks MUST be deleted in `onExit()` before activity destruction
- Member `FsFile` handles MUST be closed in `onExit()` (local `FsFile` variables auto-close via destructor)

**Activity Pattern**:

```cpp
void onEnter()  { Activity::onEnter(); /* alloc: buffer, tasks */ render(); }
void loop()     { mappedInput.update(); /* handle input */ }
void onExit()   { /* free: vTaskDelete, free buffer, close member FsFiles */ Activity::onExit(); }
```

**Critical**: Free resources in reverse order. Delete tasks BEFORE activity destruction.

### Modular Apps & Plugin Framework (`src/apps/`)

This fork introduces an extensible, registry-based modular app subsystem. To ensure easy upstream rebasing, all custom apps and plugins reside completely isolated inside `src/apps/`, minimizing touchpoints to the upstream codebase.

#### Architecture

```text
src/apps/
├── AppDescriptor.h         # App contract and metadata (Flash-resident)
├── AppRegistry.h/.cpp      # Static catalog of registered apps
├── AppsActivity.h/.cpp     # FreeInkUI launcher for browsing and launching apps
└── <your_app>/             # Self-contained app implementation (e.g., chess/)
```

#### How to Develop a New App

1. **Create App Directory**:
   Create a dedicated directory under `src/apps/<app_id>/` (e.g., `src/apps/calculator/` or `src/apps/chess/`).
2. **Implement an Activity Subclass**:
   Derive your app from [`Activity`](file:///home/chaz_bailey/workspace/crosspoint-reader/src/activities/Activity.h) (or [`UiAppHost`](file:///home/chaz_bailey/workspace/crosspoint-reader/src/components/UiAppHost.h) / [`UiListActivity`](file:///home/chaz_bailey/workspace/crosspoint-reader/src/activities/UiListActivity.h)):
   ```cpp
   class MyAppActivity final : public Activity {
    public:
     MyAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
         : Activity("MyApp", renderer, mappedInput) {}
     void onEnter() override;
     void onExit() override;
     void loop() override;
     void render(RenderLock&&) override;
   };
   ```
3. **Register the App in [`src/apps/AppRegistry.cpp`](file:///home/chaz_bailey/workspace/crosspoint-reader/src/apps/AppRegistry.cpp)**:
   Add a descriptor entry to `registry()` in `AppRegistry.cpp`:
   ```cpp
   #include "myapp/MyAppActivity.h"

   // In registry():
   {
       "myapp",                             // Unique ID
       "My App Name",                      // Title (or localized via getTitle)
       "Description of what this app does",// Subtitle
       nullptr,                            // Optional getTitle() fn: []() { return tr(STR_...); }
       nullptr,                            // Optional getDescription() fn
       UIIcon::Blocks,                     // Icon for the launcher list
       [](GfxRenderer& r, MappedInputManager& in) -> std::unique_ptr<Activity> {
         return std::make_unique<MyAppActivity>(r, in);
       },
   },
   ```
4. **App Guidelines**:
   - **Offline-First & Battery Discipline**: If your app uses network features, keep the Wi-Fi radio turned off (`WIFI_OFF`) by default. Bring up Wi-Fi on-demand for synchronization, then immediately disconnect and turn off the radio (`WiFi.disconnect(true); WiFi.mode(WIFI_OFF);`).
   - **SD Card Storage**: Store app data, caches, or state in a dedicated folder under `/.crosspoint/apps/<app_id>/` via `HalStorage`.
   - **Dual Input**: Support both capacitive touch (`mappedInput.wasScreenTapped(x, y)`) and physical 5-button D-pad controls (`mappedInput.wasPressed(...)`).
   - **E-Ink Refresh**: Use `HalDisplay::FAST_REFRESH` for interactive UI updates to avoid full-screen flashing.

### Lua Application Subsystem (`src/apps/lua/` & `src/apps/installer/`)

In addition to compiled C++ activities, CrossPoint supports dynamic, sandboxed **Lua 5.4 applications** stored on the SD card under `/apps/<app_id>/`. This enables rapid prototyping, community app development without firmware re-flashing, and an on-device App Store.

#### Architecture

```text
src/apps/
├── AppDescriptor.h         # App contract: title, icon, orientation, sleep screen hooks
├── AppRegistry.h/.cpp      # Aggregates C++ apps and discovered SD Lua apps
├── AppsActivity.h/.cpp     # FreeInkUI launcher for browsing and launching apps
├── installer/              # On-device App Store / Package Manager
│   ├── AppCatalog.h/.cpp   # Multi-repo catalog parser (catalog.json)
│   ├── AppInstaller.h/.cpp # HTTPS downloader & file unpacker
│   ├── AppSourceStore.h/.cpp # Persisted GitHub repositories list
│   └── AppStoreActivity.h/.cpp # FreeInkUI store interface
└── lua/                    # Lua 5.4 runtime and hardware bindings
    ├── LuaPsramAlloc.h     # Heap allocator mapping all Lua memory to 8MB PSRAM
    ├── LuaAppScanner.h/.cpp# Discovers SD apps and standalone .lua scripts
    ├── LuaAppActivity.h/.cpp# Activity wrapper running the Lua lifecycle
    └── LuaBindings.h/.cpp  # gfx, input, storage, crosspoint C++ bindings
```

#### Application Lifecycle & Callbacks

Lua applications define the following global callback functions:
- `onEnter()`: Called when the app starts. Read saved state with `storage.readFile()`.
- `onTouch(x, y)`: Triggered on capacitive touchscreen tap with logical display coordinates.
- `onInput(buttonId, isDown)`: Triggered on hardware button press (`input.BTN_UP`, `BTN_DOWN`, `BTN_CONFIRM`, etc.).
- `onBack()`: Optional back-button handler. Return `true` to consume the back event (e.g. closing a dialog/sub-menu), or return `false`/nil to let CrossPoint exit the app.
- `onUpdate(dt)`: Called periodically (~50ms) for animations or timers.
- `onDraw()`: Primary rendering function. Clear the screen and draw UI elements using `gfx.*`.
- `onSleepDraw()`: Low-power sleep screen renderer. Invoked when the reader enters sleep if this app was designated as the sleep app via `crosspoint.setSleepApp("<app_id>")`.
- `onExit()`: Cleanup before the Lua state is closed.

#### Display Orientation Protocol

On CrossPoint hardware, the default orientation is **Portrait** ($480 \text{ wide} \times 800 \text{ high}$). However, apps such as chessboards or data tables require **Landscape** ($800 \text{ wide} \times 480 \text{ high}$).

1. **Manifest Declaration**: Declare `"orientation"` in `/apps/<app_id>/manifest.json`:
   ```json
   {
     "id": "chess",
     "title": "Daily Chess",
     "orientation": "landscape",
     "sleepScreen": true
   }
   ```
   Valid values: `"portrait"` (default), `"landscape"` (`"landscape_cw"`), `"portrait_inverted"`, `"landscape_ccw"`.
2. **Firmware Auto-Rotation**:
   - In `LuaAppActivity::onEnter()`, the original orientation is saved (`origOrientation_ = renderer.getOrientation()`), and the requested orientation is applied to `renderer`.
   - In `LuaAppActivity::onExit()`, `renderer.setOrientation(origOrientation_)` is guaranteed to be restored.
   - In `LuaAppActivity::renderSleepScreen()`, the display is temporarily set to the app's declared orientation, `onSleepDraw` executes, and the previous orientation is restored.
3. **Runtime API**: Apps can also inspect or alter orientation dynamically:
   - `gfx.setOrientation("landscape")` or `gfx.setOrientation(gfx.ORIENTATION_LANDSCAPE)`
   - `gfx.getOrientation()` returns `"portrait"` or `"landscape"`
4. **Coordinate Rotation**: `GfxRenderer` automatically rotates all drawing primitives (`drawRect`, `drawText`, `drawSprite`) and touch input coordinates (`tapToLogical`). Apps must always query `gfx.getWidth()` and `gfx.getHeight()` rather than hardcoding 800 or 480.

#### Critical Architecture Rules for Lua

1. **PSRAM Allocation**: All Lua heap memory MUST be allocated through `luaPsramAlloc` (in `LuaPsramAlloc.h`), which wraps `heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`. Internal DRAM must never be depleted by script runtimes.
2. **`LUA_32BITS=1` 24-bit Float Mantissa Hazard**:
   Under `-DLUA_32BITS=1`, Lua's default number type is a 32-bit single-precision float (`float`) with only 24 bits of mantissa. Converting any 32-bit integer $> 2^{24}$ (such as CrossPoint's font hash IDs, e.g. `UI_10_FONT_ID = 1322569422`) via `lua_tonumber` / `luaL_checknumber` silently corrupts the lower bits, resulting in font misses and blank text.
   **Rule**: Always use `luaL_checkinteger` for font IDs and handles, and in generic integer helpers check `if (lua_isinteger(L, arg))` first before falling back to `luaL_checknumber()`.
3. **Mutex Serialization on SD Card**: The `storage` module in `LuaBindings.cpp` must strictly route file I/O through `HalStorage` (`Storage.openFileForRead`, `Storage.openFileForWrite`), never raw `SdFat`.
4. **Thread-Safety & Multi-Task Synchronization (`luaMutex_`)**:
   `LuaAppActivity` serializes all access to the `lua_State*` using `std::recursive_mutex luaMutex_`. Because `FreeInkUI` invokes `onDraw()` from the background `renderTaskLoop` task while touch taps (`onTouch()`) and hardware button events (`onInput()`, `onBack()`) arrive on the main loop task, any unsynchronized concurrent access corrupts the Lua VM stack, triggering CPU panics (`LoadProhibited` in `luaV_execute`). Every Lua lifecycle callback invocation and resource release MUST acquire `luaMutex_`.
5. **Modular Architecture & Custom Package Searcher**:
   `registerBindings()` injects `appModuleSearcher` at `package.searchers[2]`. This enables apps to split complex code across submodules and directories (e.g. `require("state")` or `require("views.grid")`) resolved relative to `/apps/<app_id>/`.
6. **Serial Logging & Stack Tracebacks**:
   The `log` module (`log.debug`, `log.info`, `log.warn`, `log.error`) routes directly to hardware serial logging (`LOG_DBG`, `LOG_INF`, `LOG_ERR`) at 115200 baud. In `LuaAppActivity::callLuaFunction()`, an error handler invokes `luaL_traceback()` so script runtime errors log the full call stack with file names and line numbers to the serial console.
7. **Color Normalization (`isColorBlack`)**:
   Both hardware firmware (`Color::White = 0x01`, `Color::Black = 0x10`) and desktop simulator (`Color::Black = 0`, `Color::White = 3`) share `isColorBlack(lua_State* L, int idx)`. Lua booleans (`true`=black, `false`=white) and integer color enums are normalized consistently across `drawText`, `drawCenteredText`, `drawRoundedRect`, and `fillRoundedRect`.
8. **Memory Profiling**:
   `crosspoint.getMemoryInfo()` provides real-time heap metrics: `{ luaMemoryKb = <int>, freeHeapKb = <int>, freePsramKb = <int> }`.
9. **On-Demand Wi-Fi Lifecycle & Battery Safety**:
   - `crosspoint.withWifi(callback)` connects on-demand using modal `WifiSelectionActivity` (temporarily rotating to Portrait if the app is in Landscape, then restoring previous orientation).
   - Once the callback completes or raises an unhandled error, `LuaAppActivity` automatically calls `WiFi.disconnect(false)` to prevent battery drain.
   - `LuaAppActivity::onExit()` and `handleLuaError()` also guarantee Wi-Fi is disconnected if it was started by the app.

### FreeRTOS Task Guidelines

**Source**: [src/activities/util/KeyboardEntryActivity.cpp:45-50](src/activities/util/KeyboardEntryActivity.cpp)

**Pattern**: See Activity Lifecycle above. `xTaskCreate(&taskTrampoline, "Name", stackSize, this, 1, &handle)`

**Stack Sizing** (in BYTES, not words):

- **2048**: Simple rendering (most activities)
- **4096**: Network, EPUB parsing
- Monitor: `uxTaskGetStackHighWaterMark()` if crashes

**Rules**: Always `vTaskDelete()` in `onExit()` before destruction. Use mutex if shared state.

### Global Font Loading

**Source**: [src/main.cpp:40-115](src/main.cpp)

**All fonts are loaded as global static objects** at firmware startup:

- Noto Serif: 12, 14, 16, 18pt (4 styles each: regular, bold, italic, bold-italic)
- Noto Sans: 12, 14, 16, 18pt (4 styles each)
- Ubuntu UI fonts: 10, 12pt (2 styles)

**Total**: ~80+ global `EpdFont` and `EpdFontFamily` objects

**Compilation Flag**:

```cpp
#ifndef OMIT_FONTS
  // Most fonts loaded here
#endif
```

**Implications**:

- Fonts stored in **Flash** (marked as `static const` in `lib/EpdFont/builtinFonts/`)
- Font rendering data cached in **DRAM** when first used
- `OMIT_FONTS` can reduce binary size for minimal builds
- Font IDs defined in [src/fontIds.h](src/fontIds.h)

**Usage**:

```cpp
#include "fontIds.h"

renderer.insertFont(FONT_UI_MEDIUM, ui12FontFamily);
renderer.drawText(FONT_UI_MEDIUM, x, y, "Hello", true);
```

---

## Testing and Debugging

### Build Commands

**Via CLI**:

```bash
# Build firmware (default environment)
pio run

# Build and upload to device
pio run -t upload

# Build specific environment
pio run -e gh_release

# Clean build artifacts
pio run -t clean
```

**Via VS Code**:

* Use PlatformIO toolbar: Build (✓), Upload (→), Clean (🗑️)
* Or Command Palette: `PlatformIO: Build`, `PlatformIO: Upload`, etc.

### Monitoring and Debugging

```bash
# Enhanced monitor with color/logging (recommended)
python3 scripts/debugging_monitor.py

# Standard PlatformIO monitor
pio device monitor
```

**Via VS Code**: Click Monitor (🔌) button in PlatformIO toolbar

### Code Quality

```bash
# Static analysis (cppcheck)
pio check

# Format only Git-modified C/C++ files, on every host
./bin/clang-format-fix -g
```

Do not run raw `clang-format` or probe it with `command -v`; use the wrapper even for diagnostics.

### Debugging Crashes

**Common Crash Causes**:

1. **Out of Memory** (Most common):
   
   ```cpp
   LOG_DBG("MEM", "Free heap: %d bytes", ESP.getFreeHeap());
   ```
   
   - Monitor heap usage throughout activity lifecycle
   
   - Check if large allocations (>10KB) occur before crash
   
   - Verify buffers are freed in `onExit()`

2. **Stack Overflow**:
   
   ```cpp
   LOG_DBG("TASK", "Stack high water: %d", uxTaskGetStackHighWaterMark(taskHandle));
   ```
   
   - Occurs during deep recursion or large local variables
   
   - Increase task stack size in `xTaskCreate()` (2048 → 4096)
   
   - Move large buffers to heap with malloc

3. **Use-After-Free**:
   
   - Activity deleted but task still running
   
   - Always `vTaskDelete()` in `onExit()` BEFORE activity destruction
   
   - Set pointers to `nullptr` after `free()`

4. **Corrupt Cache Files**:
   
   - Delete `.crosspoint/` directory on SD card
   
   - Forces clean re-parse of all EPUBs
   
   - Check file format versions in [docs/file-formats.md](docs/file-formats.md)

5. **Watchdog Timeout**:
   
   - Loop/task blocked for >5 seconds
   
   - Add `vTaskDelay(1)` in tight loops
   
   - Check for blocking I/O operations

**Verification Steps**:

1. Check serial output for stack traces
2. Monitor heap with `ESP.getFreeHeap()` before/after operations
3. Verify task deletion with task list (`vTaskList()`)
4. Test with `LOG_LEVEL=2` (debug logging enabled)

---

## Git Workflow and Repository Awareness

### Repository Detection Protocol

**CRITICAL**: ALWAYS verify repository context before git operations. This could be:

- A **fork** with `origin` pointing to personal repo, `upstream` to main repo
- A **direct clone** with `origin` pointing to main repo
- Multiple collaborator remotes

**Verification Commands** (run at session start):

```bash
# Check current branch
git branch --show-current

# Check all remotes
git remote -v

# Check working tree status
git status --short
```

**Example Output** (forked repository):

```text
origin      https://github.com/<your-username>/crosspoint-reader.git (fetch/push)
upstream    https://github.com/crosspoint-reader/crosspoint-reader.git (fetch/push)
```

### Git Operation Rules

1. Integration branches and PR comparisons target `develop`, not `master` or the remote's symbolic HEAD.
2. Never push to any remote or open/close a PR without explicit user approval. Complete local work and any requested local commit, then stop.
3. If the user explicitly approves a push, inspect remotes again and use `fork` for the feature branch unless the user specifies otherwise.
4. Never add Claude, Codex, or assistant self-attribution as a commit co-author or generated-by trailer.
5. When a change supersedes or adapts another person's PR, verify the original human author from Git/GitHub and add that person as `Co-Authored-By`; skip bot authors.

### Fork Maintenance & Upstream Sync Workflow

This repository operates as an ESP32-S3 feature fork maintained on the `custom-crosspoint` branch.

#### Branch Structure

* **`upstream/develop`**: Canonical upstream branch (`https://github.com/crosspoint-reader/crosspoint-reader.git`).
* **`develop`**: A pristine, commit-free mirror of `upstream/develop` (tracks `upstream/develop`). Never commit directly to `develop`.
* **`custom-crosspoint`**: The active branch containing fork customizations (ESP32-S3 support, modular apps framework, etc.), tracking `origin/custom-crosspoint`.

#### Rebase Policy (No Merge Commits)

Do **not** use GitHub's web "Sync fork" button or `git merge upstream/develop` into customization branches. Doing so generates unnecessary merge commits and pollutes git history with divergent bubbles. Always rebase custom commits on top of upstream updates to maintain a clean linear history.

#### Routine Upstream Sync

To pull upstream updates and rebase custom work cleanly:

```bash
# 1. Update develop cleanly from upstream (fast-forward)
git checkout develop
git pull

# 2. Rebase custom branch onto updated develop
git checkout custom-crosspoint
git rebase develop

# 3. Push rebased changes to fork
git push --force-with-lease origin custom-crosspoint

# 4. (Optional) Keep GitHub fork's develop in sync with upstream
git checkout develop && git push origin develop
```

### Branch Naming Convention

**For feature/fix branches**:

```text
feature/<short-description>       # New features
fix/<issue-number>-<description>  # Bug fixes
refactor/<component-name>         # Code refactoring
docs/<topic>                      # Documentation updates
```

**Examples**:

- `feature/sd-download-progress`
- `fix/123-orientation-crash`
- `refactor/hal-storage`

### Commit Message Format

**Pattern**:

```text
<type>: <short summary (50 chars max)>

<optional detailed description>
```

**Types**: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

**Example**:

```text
feat: add real-time SD download progress bar

Implements progress tracking for book downloads using
UITheme progress bar component with heap-safe updates.

Tested in all 4 orientations with 5MB+ files.
```

### When to Commit

**DO commit when**:

- User explicitly requests: "commit these changes"
- Feature is complete and tested on device
- Bug fix is verified working
- Refactoring preserves all functionality
- All tests pass (`pio run` succeeds)

**DO NOT commit when**:

- Changes are untested on actual hardware
- Build fails or has warnings
- Experimenting or debugging in progress
- User hasn't explicitly requested commit
- Files excluded by `.gitignore` would be included — always run `git status` and cross-check against `.gitignore` before staging (e.g., `*.generated.h`, `.pio/`, `compile_commands.json`, `platformio.local.ini`)

**Rule**: **If uncertain, ASK before committing.**

---

## Generated Files and Build Artifacts

### Files Generated by Build Scripts

**NEVER manually edit these files** - they are regenerated automatically:

1. **HTML Headers** (generated by `scripts/build_html.py`):
   
   - `src/network/html/*.generated.h`
   
   - **Source**: HTML templates in `data/html/` directory
   
   - **Triggered**: During PlatformIO `pre:` build step
   
   - **To modify**: Edit source HTML in `data/html/`, not generated headers

2. **I18n Headers** (generated by `scripts/gen_i18n.py`):
   
   - `lib/I18n/I18nKeys.h`, `lib/I18n/I18nStrings.h`, `lib/I18n/I18nStrings.cpp`
   
   - **Source**: YAML translation files in `lib/I18n/translations/` (one per language)
   
   - **To modify**: Edit source YAML files, then run `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`
   
   - **Commit**: Source YAML files only. All three generated files (`I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp`) are in `.gitignore` and regenerated at build time.

3. **Build Artifacts** (in `.gitignore`):
   
   - `.pio/` - PlatformIO build output
   
   - `build/` - Compiled binaries
   
   - `*.generated.h` - Any auto-generated headers
   
   - `compile_commands.json` - LSP/IDE metadata

### Modifying Generated Content Workflow

**To change HTML pages**:

1. Edit source: `data/html/<pagename>.html`
2. Build: `pio run` (auto-triggers `scripts/build_html.py`)
3. Generated headers update: `src/network/html/<pagename>Html.generated.h`
4. **Commit ONLY** source HTML, NOT generated `.generated.h` files

**To add/modify translations (i18n)**:

1. Edit or add YAML file: `lib/I18n/translations/<language>.yaml`
   - Each file must contain: `_language_name`, `_language_code`, `_order`, `_bcp47`, and `STR_*` keys
   - English (`english.yaml`) is the reference; missing keys in other languages fall back to English
2. Run generator: `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`
3. Generated files update: `I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp`
4. **Commit** source YAML files only. All three generated files are in `.gitignore` and regenerated at build time.

**To use translated strings in code**:

```cpp
#include <I18n.h>
// Use tr() macro with StrId enum (defined in generated I18nKeys.h)
renderer.drawText(FONT_UI, x, y, tr(STR_LOADING), true);
```

**To add custom fonts**:

1. Place source fonts in `lib/EpdFont/fontsrc/` (gitignored)
2. Run conversion script (see `lib/EpdFont/README`)
3. Update global font objects in `src/main.cpp:40-115`
4. Add font ID constant to `src/fontIds.h`

---

## Local Development Configuration

### platformio.local.ini (Personal Overrides)

**Purpose**: Personal development settings that should NEVER be committed.

**Use Cases**:

- Serial port configuration (varies by machine)
- Debug flags for specific testing
- Local build optimizations
- Developer-specific paths

**Example** `platformio.local.ini`:

```ini
# platformio.local.ini (gitignored)
[env:default]
upload_port = COM7              # Windows: COMx, Linux: /dev/ttyUSBx
monitor_port = COM7

build_flags =
  ${base.build_flags}
  -DMY_DEBUG_FLAG=1             # Personal debug flags
  -DTEST_FEATURE_ENABLED=1
```

**Configuration Hierarchy**:

1. `platformio.ini` - **Committed**, shared project settings
2. `platformio.local.ini` - **Gitignored**, personal overrides
3. Local file extends/overrides base config

**Rules**:

- **NEVER commit** `platformio.local.ini`
- **NEVER put** personal info (serial ports, credentials) in main `platformio.ini`
- Use `${base.build_flags}` to extend (not replace) base flags

---

## Testing and Verification Workflow

### Testing Checklist

**AI agent scope** (what you CAN verify):

1. ✅ **Build**: Build once after the last code edit with the relevant `pio run` target. Do not clean by default, repeat a target that already passed, or rebuild after formatting/comment-only/documentation-only changes.
2. ✅ **Quality**: `pio check` when relevant + `./bin/clang-format-fix -g`
3. ✅ **Format**: Commit messages (`feat:`/`fix:`), no `.gitignore`-excluded files staged (e.g., `*.generated.h`, `.pio/`, `platformio.local.ini`)
4. ✅ **CI**: Fix GitHub Actions failures before review
5. ✅ **Code review**: Ensure orientation-aware logic is correct in all 4 modes by inspecting switch/case coverage

**Human tester scope** (flag these for the user):
6. 🔲 **Device**: Test on hardware
7. 🔲 **Orientations**: Verify all 4 modes (Portrait/Inverted/Landscape CW/CCW)
8. 🔲 **Heap**: `ESP.getFreeHeap()` > 50KB, no leaks
9. 🔲 **Cache**: If EPUB modified, delete `.crosspoint/` and verify re-parse

### CI/CD Pipeline Awareness

**GitHub Actions** run automatically on pull requests:

| Workflow      | File                                        | Purpose                |
| ------------- | ------------------------------------------- | ---------------------- |
| Build Check   | `.github/workflows/ci.yml`                  | Verifies code compiles |
| Format Check  | `.github/workflows/pr-formatting-check.yml` | Validates clang-format |
| Release Build | `.github/workflows/release.yml`             | Production releases    |
| RC Build      | `.github/workflows/release_candidate.yml`   | Release candidates     |

**Rules**:

- **Fix CI failures BEFORE** requesting review
- CI runs on: Push to PR, PR updates
- Format check fails → Run `./bin/clang-format-fix -g`
- Build check fails → Fix compile errors

---

## Serial Monitoring and Live Debugging

### Serial Monitor Options

1. **Enhanced**: `python3 scripts/debugging_monitor.py` (color-coded, recommended)
2. **Standard**: `pio device monitor` (basic, no colors)
3. **VS Code**: Monitor (🔌) button (IDE-integrated)

### Live Debugging Patterns

**Heap**: `LOG_DBG("MEM", "Free: %d", ESP.getFreeHeap());` (every 5s in loop)
**Stack**: `uxTaskGetStackHighWaterMark(nullptr)` (< 512 bytes → increase stack)
**Flush**: `logSerial.flush();` (force output before crash)

**Port Detection**: Windows: `mode` | Linux: `ls /dev/ttyUSB* /dev/ttyACM*` or `dmesg | grep tty`

---

## Cache Management and Invalidation

### Cache Structure on SD Card

**Location**: `.crosspoint/` directory on SD card root

**Structure**: `.crosspoint/epub_<hash>/{book.bin, progress.bin, cover.bmp, sections/*.bin}`

**Hash**: `std::hash<std::string>{}(filepath)` → Moving/renaming file = new hash = lost progress

### Cache Invalidation Rules

**Cache is automatically invalidated when**:

1. **File format version changes** (see `docs/file-formats.md`)
   
   - `book.bin` version number incremented
   
   - `section.bin` version number incremented
2. **Render settings change**:
   
   - Font family or size (`SETTINGS.fontFamily`, `SETTINGS.fontSize`)
   
   - Line spacing (`SETTINGS.lineSpacing`)
   
   - Paragraph spacing (`SETTINGS.extraParagraphSpacing`)
   
   - Screen margins (`SETTINGS.screenMargin`)
3. **Viewport dimensions change**:
   
   - Screen orientation change
   
   - Display resolution change
4. **Book file modified**:
   
   - Moved, renamed, or content changed (new hash)

**Manual Cache Clear** (safe operations):

```bash
# Delete ALL caches (forces full regeneration)
rm -rf /path/to/sd/.crosspoint/

# Delete specific book cache
rm -rf /path/to/sd/.crosspoint/epub_<hash>/

# Keep progress, delete only rendered sections
rm -rf /path/to/sd/.crosspoint/epub_<hash>/sections/
```

**When to Clear Cache**:

- EPUB parsing errors after code changes to `lib/Epub/`
- Corrupt rendering (missing text, wrong layout)
- Testing cache generation logic
- After modifying:
  - `lib/Epub/Epub/Section.cpp`
  - `lib/Epub/Epub/BookMetadataCache.cpp`
  - Render settings in `CrossPointSettings`

### Cache File Format Versioning

**Source**: `lib/Epub/Epub/Section.cpp`, `lib/Epub/Epub/BookMetadataCache.cpp`

**Current Versions** (as of docs/file-formats.md):

- `book.bin`: **Version 7** (metadata structure)
- `section.bin`: **Version 25** (layout structure)

**Version Increment Rules**:

1. **ALWAYS increment version** BEFORE changing binary structure
2. Version mismatch → Cache auto-invalidated and regenerated
3. Document format changes in `docs/file-formats.md`

**Example** (incrementing section format version):

```cpp
// lib/Epub/Epub/Section.cpp
static constexpr uint8_t SECTION_FILE_VERSION = 26;  // Was 25, now 26

// Add new field to structure
struct PageLine {
  // ... existing fields ...
  uint16_t newField;  // New field added
};
```

---

Philosophy: We are building a dedicated e-reader, not a Swiss Army knife. If a feature adds RAM pressure without significantly improving the reading experience, it is Out of Scope.
