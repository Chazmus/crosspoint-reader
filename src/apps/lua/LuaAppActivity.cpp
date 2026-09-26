#include "LuaAppActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include "LuaPsramAlloc.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "apps/AppDescriptor.h"
#include "fontIds.h"

LuaAppActivity::LuaAppActivity(std::string scriptPath, std::string appDir, std::string appTitle,
                               std::string orientationStr, GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("LuaApp", renderer, mappedInput),
      scriptPath_(std::move(scriptPath)),
      appDir_(std::move(appDir)),
      appTitle_(std::move(appTitle)),
      orientationStr_(std::move(orientationStr)) {}

LuaAppActivity::~LuaAppActivity() {
  std::lock_guard<std::recursive_mutex> lock(luaMutex_);
  if (L_) {
    if (wifiCallbackRef_ != LUA_NOREF) {
      luaL_unref(L_, LUA_REGISTRYINDEX, wifiCallbackRef_);
      wifiCallbackRef_ = LUA_NOREF;
    }
    lua_close(L_);
    L_ = nullptr;
  }
}

namespace {

int luaTraceback(lua_State* L) {
  const char* msg = lua_tostring(L, 1);
  if (!msg) {
    if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING) {
      return 1;
    }
    msg = "(error object is not a string)";
  }
  luaL_traceback(L, L, msg, 1);
  return 1;
}

}  // namespace

void LuaAppActivity::handleLuaError(const char* context) {
  hasError_ = true;
  const char* msg = L_ ? lua_tostring(L_, -1) : "Unknown Lua error";
  errorMessage_ = msg ? msg : "Unknown Lua error";
  LOG_ERR("LUA", "[%s] %s", context, errorMessage_.c_str());
  if (L_) lua_pop(L_, 1);
#if defined(ESP32) || defined(ARDUINO)
  if (wifiStartedByUs_ && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    wifiStartedByUs_ = false;
  }
#endif
}

bool LuaAppActivity::callLuaFunction(const char* funcName) {
  std::lock_guard<std::recursive_mutex> lock(luaMutex_);
  if (!L_ || hasError_) return false;
  const int errIdx = lua_gettop(L_) + 1;
  lua_pushcfunction(L_, luaTraceback);
  lua_getglobal(L_, funcName);
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 2);
    return false;
  }
  const unsigned long t0 = millis();
  if (lua_pcall(L_, 0, 0, errIdx) != LUA_OK) {
    handleLuaError(funcName);
    lua_remove(L_, errIdx);
    requestUpdate();
    return false;
  }
  lua_remove(L_, errIdx);
  const unsigned long elapsed = millis() - t0;
  if (elapsed > 100) {
    LOG_INF("LUA_PERF", "[%s] took %lu ms (Lua RAM: %d KB)", funcName, elapsed, lua_gc(L_, LUA_GCCOUNT, 0));
  }
  return true;
}

bool LuaAppActivity::callLuaFunction(const char* funcName, int arg1, int arg2) {
  std::lock_guard<std::recursive_mutex> lock(luaMutex_);
  if (!L_ || hasError_) return false;
  const int errIdx = lua_gettop(L_) + 1;
  lua_pushcfunction(L_, luaTraceback);
  lua_getglobal(L_, funcName);
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 2);
    return false;
  }
  lua_pushinteger(L_, arg1);
  lua_pushinteger(L_, arg2);
  const unsigned long t0 = millis();
  if (lua_pcall(L_, 2, 0, errIdx) != LUA_OK) {
    handleLuaError(funcName);
    lua_remove(L_, errIdx);
    requestUpdate();
    return false;
  }
  lua_remove(L_, errIdx);
  const unsigned long elapsed = millis() - t0;
  if (elapsed > 100) {
    LOG_INF("LUA_PERF", "[%s] took %lu ms (Lua RAM: %d KB)", funcName, elapsed, lua_gc(L_, LUA_GCCOUNT, 0));
  }
  return true;
}

bool LuaAppActivity::callLuaFunction(const char* funcName, float arg1) {
  std::lock_guard<std::recursive_mutex> lock(luaMutex_);
  if (!L_ || hasError_) return false;
  const int errIdx = lua_gettop(L_) + 1;
  lua_pushcfunction(L_, luaTraceback);
  lua_getglobal(L_, funcName);
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 2);
    return false;
  }
  lua_pushnumber(L_, arg1);
  if (lua_pcall(L_, 1, 0, errIdx) != LUA_OK) {
    handleLuaError(funcName);
    lua_remove(L_, errIdx);
    requestUpdate();
    return false;
  }
  lua_remove(L_, errIdx);
  return true;
}

void LuaAppActivity::onEnter() {
  Activity::onEnter();
  std::lock_guard<std::recursive_mutex> lock(luaMutex_);
  hasError_ = false;
  errorMessage_.clear();

  origOrientation_ = renderer.getOrientation();
  if (orientationStr_ == "landscape" || orientationStr_ == "landscape_cw") {
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
  } else if (orientationStr_ == "landscape_ccw") {
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  } else if (orientationStr_ == "portrait_inverted") {
    renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
  } else {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  }

  L_ = lua_newstate(luaPsramAlloc, nullptr);
  if (!L_) {
    LOG_ERR("LUA", "Failed to allocate Lua state in PSRAM");
    hasError_ = true;
    errorMessage_ = "Failed to allocate Lua state in PSRAM";
    requestUpdate();
    return;
  }

  luaL_openlibs(L_);

  hostCtx_.renderer = &renderer;
  hostCtx_.input = &mappedInput;
  hostCtx_.activity = this;
  hostCtx_.luaApp = this;
  hostCtx_.appDir = appDir_;
  lua_host::registerBindings(L_, &hostCtx_);

  // Read script file from SD card
  HalFile file;
  if (!Storage.openFileForRead("LUA", scriptPath_, file)) {
    LOG_ERR("LUA", "Failed to open script: %s", scriptPath_.c_str());
    hasError_ = true;
    errorMessage_ = "Failed to open script: " + scriptPath_;
    requestUpdate();
    return;
  }

  const size_t sz = file.size();
  std::string scriptContent;
  scriptContent.resize(sz);
  if (sz > 0) {
    file.read(&scriptContent[0], sz);
  }

  // Load and execute script chunk
  const int errIdx = lua_gettop(L_) + 1;
  lua_pushcfunction(L_, luaTraceback);

  if (luaL_loadbuffer(L_, scriptContent.data(), scriptContent.size(), scriptPath_.c_str()) != LUA_OK) {
    handleLuaError("LoadScript");
    lua_remove(L_, errIdx);
    requestUpdate();
    return;
  }

  if (lua_pcall(L_, 0, 0, errIdx) != LUA_OK) {
    handleLuaError("InitScript");
    lua_remove(L_, errIdx);
    requestUpdate();
    return;
  }
  lua_remove(L_, errIdx);

  callLuaFunction("onEnter");
  lastUpdateMs_ = millis();
  requestUpdate();
}

void LuaAppActivity::loop() {
  if (hasError_) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    bool consumed = false;
    {
      std::lock_guard<std::recursive_mutex> lock(luaMutex_);
      if (L_) {
        const int errIdx = lua_gettop(L_) + 1;
        lua_pushcfunction(L_, luaTraceback);
        lua_getglobal(L_, "onBack");
        if (lua_isfunction(L_, -1)) {
          if (lua_pcall(L_, 0, 1, errIdx) == LUA_OK) {
            consumed = lua_toboolean(L_, -1);
            lua_pop(L_, 1);
            lua_remove(L_, errIdx);
          } else {
            handleLuaError("onBack");
            lua_remove(L_, errIdx);
            requestUpdate();
            return;
          }
        } else {
          lua_pop(L_, 2);
        }
      }
    }
    if (consumed) return;
    finish();
    return;
  }

  int downX = 0, downY = 0;
  if (mappedInput.wasScreenTouchDown(downX, downY)) {
    callLuaFunction("onTouchDown", downX, downY);
  }

  int tapX = 0, tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    callLuaFunction("onTouch", tapX, tapY);
  }

  if (mappedInput.wasScreenTouchReleased()) {
    callLuaFunction("onTouchUp");
  }

  static constexpr MappedInputManager::Button btns[] = {
      MappedInputManager::Button::Confirm, MappedInputManager::Button::Left, MappedInputManager::Button::Right,
      MappedInputManager::Button::Up,      MappedInputManager::Button::Down,
  };

  for (const auto b : btns) {
    if (mappedInput.wasPressed(b)) {
      callLuaFunction("onInput", static_cast<int>(b), 1);
    }
  }

  const unsigned long now = millis();
  if (now - lastUpdateMs_ >= 50) {
    const float dt = (now - lastUpdateMs_) / 1000.0f;
    lastUpdateMs_ = now;
    callLuaFunction("onUpdate", dt);
  }
}

void LuaAppActivity::render(RenderLock&&) {
  if (hasError_) {
    renderer.clearScreen();
    const int w = renderer.getScreenWidth();
    const int h = renderer.getScreenHeight();

    renderer.drawRect(20, 20, w - 40, h - 40, 2, true);
    renderer.drawText(UI_12_FONT_ID, 40, 60, "Lua Application Error", true, EpdFontFamily::BOLD);
    renderer.drawLine(40, 75, w - 40, 75, 1, true);

    if (!appTitle_.empty()) {
      renderer.drawText(SMALL_FONT_ID, 40, 95, ("App: " + appTitle_).c_str(), true);
    }

    const auto lines = renderer.wrappedText(SMALL_FONT_ID, errorMessage_.c_str(), w - 80, 10);
    int y = 125;
    for (const auto& line : lines) {
      renderer.drawText(SMALL_FONT_ID, 40, y, line.c_str(), true);
      y += renderer.getLineHeight(SMALL_FONT_ID);
    }

    renderer.drawText(UI_10_FONT_ID, 40, h - 50, "Press Back to return to launcher", true, EpdFontFamily::BOLD);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  callLuaFunction("onDraw");
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void LuaAppActivity::onExit() {
  {
    std::lock_guard<std::recursive_mutex> lock(luaMutex_);
    if (L_) {
      if (wifiCallbackRef_ != LUA_NOREF) {
        luaL_unref(L_, LUA_REGISTRYINDEX, wifiCallbackRef_);
        wifiCallbackRef_ = LUA_NOREF;
      }
      callLuaFunction("onExit");
      lua_close(L_);
      L_ = nullptr;
    }
  }
#if defined(ESP32) || defined(ARDUINO)
  if (wifiStartedByUs_ && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    wifiStartedByUs_ = false;
  }
#endif
  renderer.setOrientation(origOrientation_);
  Activity::onExit();
}

void LuaAppActivity::connectWifi(int callbackRef, bool autoDisconnect) {
#if defined(ESP32) || defined(ARDUINO)
  if (WiFi.status() == WL_CONNECTED) {
    wifiStartedByUs_ = false;
    wifiAutoDisconnect_ = autoDisconnect;
    wifiCallbackRef_ = callbackRef;
    onWifiSelectionComplete(true);
    return;
  }

  wifiStartedByUs_ = true;
  wifiAutoDisconnect_ = autoDisconnect;
  wifiCallbackRef_ = callbackRef;

  // Temporarily switch to Portrait for standard CrossPoint Wi-Fi UI & keyboard
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& res) {
                           onWifiSelectionComplete(!res.isCancelled);
                         });
#else
  wifiStartedByUs_ = false;
  wifiAutoDisconnect_ = autoDisconnect;
  wifiCallbackRef_ = callbackRef;
  onWifiSelectionComplete(true);
#endif
}

void LuaAppActivity::disconnectWifi() {
#if defined(ESP32) || defined(ARDUINO)
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
  }
#endif
  wifiStartedByUs_ = false;
}

void LuaAppActivity::onWifiSelectionComplete(const bool success) {
  // Restore Lua app's declared orientation
  if (orientationStr_ == "landscape" || orientationStr_ == "landscape_cw") {
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
  } else if (orientationStr_ == "landscape_ccw") {
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  } else if (orientationStr_ == "portrait_inverted") {
    renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
  } else {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  }

  const int cbRef = wifiCallbackRef_;
  wifiCallbackRef_ = LUA_NOREF;
  const bool autoDisconnect = wifiAutoDisconnect_;
  wifiAutoDisconnect_ = false;

  if (cbRef != LUA_NOREF) {
    std::lock_guard<std::recursive_mutex> lock(luaMutex_);
    if (L_) {
      const int errIdx = lua_gettop(L_) + 1;
      lua_pushcfunction(L_, luaTraceback);
      lua_rawgeti(L_, LUA_REGISTRYINDEX, cbRef);
      luaL_unref(L_, LUA_REGISTRYINDEX, cbRef);
      lua_pushboolean(L_, success);
      if (lua_pcall(L_, 1, 0, errIdx) != LUA_OK) {
        handleLuaError("wifiCallback");
      }
      lua_remove(L_, errIdx);
      requestUpdate();
    }
  }

  // If withWifi requested automatic disconnect, or connection failed/cancelled, disconnect now
  if ((autoDisconnect || !success) && wifiStartedByUs_) {
    disconnectWifi();
  }
}

std::unique_ptr<Activity> LuaAppActivity::create(const AppDescriptor& desc, GfxRenderer& renderer,
                                                 MappedInputManager& input) {
  return makeUniqueNoThrow<LuaAppActivity>(desc.scriptPath, desc.appDir, desc.resolveTitle(), desc.orientationStr,
                                           renderer, input);
}

bool LuaAppActivity::renderSleepScreen(const AppDescriptor& desc, GfxRenderer& renderer) {
  if (desc.scriptPath.empty()) return false;

  const auto origOrient = renderer.getOrientation();
  if (desc.orientationStr == "landscape" || desc.orientationStr == "landscape_cw") {
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
  } else if (desc.orientationStr == "landscape_ccw") {
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  } else if (desc.orientationStr == "portrait_inverted") {
    renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
  } else {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  }

  lua_State* L = lua_newstate(luaPsramAlloc, nullptr);
  if (!L) {
    renderer.setOrientation(origOrient);
    return false;
  }

  luaL_openlibs(L);

  lua_host::HostContext ctx;
  ctx.renderer = &renderer;
  ctx.appDir = desc.appDir;
  lua_host::registerBindings(L, &ctx);

  HalFile file;
  if (!Storage.openFileForRead("LUA", desc.scriptPath, file)) {
    lua_close(L);
    renderer.setOrientation(origOrient);
    return false;
  }

  const size_t sz = file.size();
  std::string scriptContent;
  scriptContent.resize(sz);
  if (sz > 0) file.read(&scriptContent[0], sz);

  if (luaL_loadbuffer(L, scriptContent.data(), scriptContent.size(), desc.scriptPath.c_str()) != LUA_OK ||
      lua_pcall(L, 0, 0, 0) != LUA_OK) {
    lua_close(L);
    renderer.setOrientation(origOrient);
    return false;
  }

  // Call onEnter if present so the app can load state from storage
  lua_getglobal(L, "onEnter");
  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_getglobal(L, "onSleepDraw");
  if (!lua_isfunction(L, -1)) {
    lua_close(L);
    renderer.setOrientation(origOrient);
    return false;
  }

  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    lua_close(L);
    renderer.setOrientation(origOrient);
    return false;
  }

  lua_close(L);
  renderer.setOrientation(origOrient);
  return true;
}
