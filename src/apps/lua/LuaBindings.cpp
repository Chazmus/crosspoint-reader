#include "LuaBindings.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include "LuaAppActivity.h"
#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include <HalClock.h>
#include <HalPowerManager.h>
#include "util/QrUtils.h"

namespace lua_host {
namespace {

HostContext* getContext(lua_State* L) { return static_cast<HostContext*>(lua_touserdata(L, lua_upvalueindex(1))); }

inline int checkInt(lua_State* L, int arg) {
  if (lua_isinteger(L, arg)) {
    return static_cast<int>(lua_tointeger(L, arg));
  }
  return static_cast<int>(luaL_checknumber(L, arg));
}

inline int optInt(lua_State* L, int arg, int def) {
  if (lua_isnoneornil(L, arg)) return def;
  if (lua_isinteger(L, arg)) {
    return static_cast<int>(lua_tointeger(L, arg));
  }
  return static_cast<int>(luaL_optnumber(L, arg, def));
}

// ---------------------------------------------------------------------------
// Gfx API
// ---------------------------------------------------------------------------

int l_gfx_getWidth(lua_State* L) {
  auto* ctx = getContext(L);
  lua_pushinteger(L, ctx && ctx->renderer ? ctx->renderer->getScreenWidth() : 800);
  return 1;
}

int l_gfx_getHeight(lua_State* L) {
  auto* ctx = getContext(L);
  lua_pushinteger(L, ctx && ctx->renderer ? ctx->renderer->getScreenHeight() : 480);
  return 1;
}

int l_gfx_setOrientation(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->renderer) return 0;

  GfxRenderer::Orientation target = GfxRenderer::Orientation::Portrait;
  if (lua_isinteger(L, 1)) {
    const int val = static_cast<int>(lua_tointeger(L, 1));
    switch (val) {
      case 1:
        target = GfxRenderer::Orientation::LandscapeClockwise;
        break;
      case 2:
        target = GfxRenderer::Orientation::PortraitInverted;
        break;
      case 3:
        target = GfxRenderer::Orientation::LandscapeCounterClockwise;
        break;
      default:
        target = GfxRenderer::Orientation::Portrait;
        break;
    }
  } else if (lua_isstring(L, 1)) {
    const char* str = lua_tostring(L, 1);
    if (strcmp(str, "landscape") == 0 || strcmp(str, "landscape_cw") == 0) {
      target = GfxRenderer::Orientation::LandscapeClockwise;
    } else if (strcmp(str, "landscape_ccw") == 0) {
      target = GfxRenderer::Orientation::LandscapeCounterClockwise;
    } else if (strcmp(str, "portrait_inverted") == 0) {
      target = GfxRenderer::Orientation::PortraitInverted;
    } else {
      target = GfxRenderer::Orientation::Portrait;
    }
  }

  ctx->renderer->setOrientation(target);
  if (ctx->activity) {
    ctx->activity->requestUpdate();
  }
  return 0;
}

int l_gfx_getOrientation(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->renderer) {
    lua_pushstring(L, "portrait");
    return 1;
  }
  switch (ctx->renderer->getOrientation()) {
    case GfxRenderer::Orientation::LandscapeClockwise:
      lua_pushstring(L, "landscape");
      break;
    case GfxRenderer::Orientation::LandscapeCounterClockwise:
      lua_pushstring(L, "landscape_ccw");
      break;
    case GfxRenderer::Orientation::PortraitInverted:
      lua_pushstring(L, "portrait_inverted");
      break;
    case GfxRenderer::Orientation::Portrait:
    default:
      lua_pushstring(L, "portrait");
      break;
  }
  return 1;
}

int l_gfx_clearScreen(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int color = luaL_optinteger(L, 1, 1);
    ctx->renderer->clearScreen(color ? 0xFF : 0x00);
  }
  return 0;
}

int l_gfx_drawPixel(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const bool black = lua_isnone(L, 3) ? true : lua_toboolean(L, 3);
    ctx->renderer->drawPixel(x, y, black);
  }
  return 0;
}

int l_gfx_drawLine(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x1 = checkInt(L, 1);
    const int y1 = checkInt(L, 2);
    const int x2 = checkInt(L, 3);
    const int y2 = checkInt(L, 4);
    const int lineWidth = optInt(L, 5, 1);
    const bool black = lua_isnone(L, 6) ? true : lua_toboolean(L, 6);
    ctx->renderer->drawLine(x1, y1, x2, y2, lineWidth, black);
  }
  return 0;
}

int l_gfx_drawRect(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const int w = checkInt(L, 3);
    const int h = checkInt(L, 4);
    const int lineWidth = optInt(L, 5, 1);
    const bool black = lua_isnone(L, 6) ? true : lua_toboolean(L, 6);
    ctx->renderer->drawRect(x, y, w, h, lineWidth, black);
  }
  return 0;
}

int l_gfx_fillRect(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const int w = checkInt(L, 3);
    const int h = checkInt(L, 4);
    const bool black = lua_isnone(L, 5) ? true : lua_toboolean(L, 5);
    ctx->renderer->fillRect(x, y, w, h, black);
  }
  return 0;
}

int l_gfx_fillRectDither(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const int w = checkInt(L, 3);
    const int h = checkInt(L, 4);
    const int col = optInt(L, 5, static_cast<int>(Color::LightGray));
    ctx->renderer->fillRectDither(x, y, w, h, static_cast<Color>(col));
  }
  return 0;
}

static bool isColorBlack(lua_State* L, int idx) {
  if (lua_isnone(L, idx)) return true;
  if (lua_isboolean(L, idx)) return lua_toboolean(L, idx);
  if (lua_isinteger(L, idx) || lua_isnumber(L, idx)) {
    const int val = lua_tointeger(L, idx);
    if (val == static_cast<int>(Color::White) || val == 1 || val == 3) {
      return false;
    }
    return true;
  }
  return lua_toboolean(L, idx);
}

int l_gfx_drawRoundedRect(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const int w = checkInt(L, 3);
    const int h = checkInt(L, 4);
    const int radius = checkInt(L, 5);
    const int lineWidth = optInt(L, 6, 1);
    const bool black = isColorBlack(L, 7);
    ctx->renderer->drawRoundedRect(x, y, w, h, lineWidth, radius, black);
  }
  return 0;
}

int l_gfx_fillRoundedRect(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const int w = checkInt(L, 3);
    const int h = checkInt(L, 4);
    const int radius = checkInt(L, 5);
    Color col = Color::Black;
    if (lua_isboolean(L, 6)) {
      col = lua_toboolean(L, 6) ? Color::Black : Color::White;
    } else if (lua_isinteger(L, 6) || lua_isnumber(L, 6)) {
      const int val = lua_tointeger(L, 6);
      if (val == static_cast<int>(Color::White) || val == 1 || val == 3) {
        col = Color::White;
      } else if (val == static_cast<int>(Color::Black) || val == 0 || val == 16) {
        col = Color::Black;
      } else {
        col = static_cast<Color>(val);
      }
    }
    ctx->renderer->fillRoundedRect(x, y, w, h, radius, col);
  }
  return 0;
}

int l_gfx_drawCircle(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int cx = checkInt(L, 1);
    const int cy = checkInt(L, 2);
    const int r = checkInt(L, 3);
    const int lineWidth = optInt(L, 4, 1);
    const bool black = isColorBlack(L, 5);
    ctx->renderer->drawArc(r, cx, cy, 0, 0, lineWidth, black);
  }
  return 0;
}

int l_gfx_drawText(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int fontId = luaL_checkinteger(L, 1);
    const int x = checkInt(L, 2);
    const int y = checkInt(L, 3);
    const char* text = luaL_checkstring(L, 4);
    const bool black = isColorBlack(L, 5);
    ctx->renderer->drawText(fontId, x, y, text, black);
  }
  return 0;
}

int l_gfx_drawCenteredText(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int fontId = luaL_checkinteger(L, 1);
    const int y = checkInt(L, 2);
    const char* text = luaL_checkstring(L, 3);
    const bool black = isColorBlack(L, 4);
    ctx->renderer->drawCenteredText(fontId, y, text, black);
  }
  return 0;
}

int l_gfx_getTextWidth(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int fontId = luaL_checkinteger(L, 1);
    const char* text = luaL_checkstring(L, 2);
    lua_pushinteger(L, ctx->renderer->getTextWidth(fontId, text));
    return 1;
  }
  lua_pushinteger(L, 0);
  return 1;
}

int l_gfx_getLineHeight(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int fontId = luaL_checkinteger(L, 1);
    lua_pushinteger(L, ctx->renderer->getLineHeight(fontId));
    return 1;
  }
  lua_pushinteger(L, 16);
  return 1;
}

int l_gfx_drawBitmapFile(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const char* relPath = luaL_checkstring(L, 3);
    const std::string fullPath = ctx->resolvePath(relPath);

    HalFile file;
    if (Storage.openFileForRead("LUA", fullPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        ctx->renderer->drawBitmap(bitmap, x, y, bitmap.getWidth(), bitmap.getHeight());
        lua_pushboolean(L, true);
        return 1;
      }
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_gfx_drawSprite(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->renderer) return 0;

  const int x = checkInt(L, 1);
  const int y = checkInt(L, 2);
  const int w = checkInt(L, 3);
  const int h = checkInt(L, 4);

  size_t inkLen = 0;
  const uint8_t* ink = reinterpret_cast<const uint8_t*>(luaL_checklstring(L, 5, &inkLen));

  size_t silLen = 0;
  const uint8_t* sil =
      lua_isnoneornil(L, 6) ? nullptr : reinterpret_cast<const uint8_t*>(luaL_checklstring(L, 6, &silLen));

  const int bytesPerRow = (w + 7) / 8;
  if (inkLen < static_cast<size_t>(bytesPerRow * h)) {
    return 0;
  }

  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      const int idx = r * bytesPerRow + (c >> 3);
      const int shift = 7 - (c & 7);
      const bool isInk = (ink[idx] >> shift) & 1;
      if (isInk) {
        ctx->renderer->drawPixel(x + c, y + r, true);
      } else if (sil && silLen >= static_cast<size_t>(bytesPerRow * h)) {
        const bool isSil = (sil[idx] >> shift) & 1;
        if (isSil) {
          ctx->renderer->drawPixel(x + c, y + r, false);
        }
      }
    }
  }
  return 0;
}

int l_gfx_drawQrCode(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int x = checkInt(L, 1);
    const int y = checkInt(L, 2);
    const int w = checkInt(L, 3);
    const int h = checkInt(L, 4);
    const char* text = luaL_checkstring(L, 5);
    QrUtils::drawQrCode(*ctx->renderer, Rect{x, y, w, h}, text);
  }
  return 0;
}

int l_gfx_displayBuffer(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->renderer) {
    const int mode = luaL_optinteger(L, 1, 0);
    HalDisplay::RefreshMode refreshMode = HalDisplay::FAST_REFRESH;
    if (mode == 1) {
      refreshMode = HalDisplay::HALF_REFRESH;
    } else if (mode == 2) {
      refreshMode = HalDisplay::FULL_REFRESH;
    }
    ctx->renderer->displayBuffer(refreshMode);
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Input API
// ---------------------------------------------------------------------------

int l_input_wasPressed(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    const int btn = luaL_checkinteger(L, 1);
    lua_pushboolean(L, ctx->input->wasPressed(static_cast<MappedInputManager::Button>(btn)));
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_input_isPressed(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    const int btn = luaL_checkinteger(L, 1);
    lua_pushboolean(L, ctx->input->isPressed(static_cast<MappedInputManager::Button>(btn)));
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_input_wasScreenTapped(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    int x = 0, y = 0;
    if (ctx->input->wasScreenTapped(x, y)) {
      lua_pushinteger(L, x);
      lua_pushinteger(L, y);
      return 2;
    }
  }
  lua_pushnil(L);
  return 1;
}

int l_input_isTouchDown(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    int x = 0, y = 0;
    lua_pushboolean(L, ctx->input->isScreenTouchHeld(x, y));
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_input_getTouch(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    int x = 0, y = 0;
    if (ctx->input->isScreenTouchHeld(x, y)) {
      lua_pushboolean(L, true);
      lua_pushinteger(L, x);
      lua_pushinteger(L, y);
      return 3;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_input_wasTouchDown(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    int x = 0, y = 0;
    if (ctx->input->wasScreenTouchDown(x, y)) {
      lua_pushboolean(L, true);
      lua_pushinteger(L, x);
      lua_pushinteger(L, y);
      return 3;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_input_wasTouchReleased(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->input) {
    lua_pushboolean(L, ctx->input->wasScreenTouchReleased());
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

// ---------------------------------------------------------------------------
// Storage API
// ---------------------------------------------------------------------------

int l_storage_readFile(lua_State* L) {
  auto* ctx = getContext(L);
  const char* relPath = luaL_checkstring(L, 1);
  const std::string fullPath = ctx ? ctx->resolvePath(relPath) : std::string(relPath);

  HalFile file;
  if (Storage.openFileForRead("LUA", fullPath, file)) {
    const size_t sz = file.size();
    if (sz > 0) {
      std::string content;
      content.resize(sz);
      const int readBytes = file.read(&content[0], sz);
      if (readBytes > 0) {
        content.resize(readBytes);
        lua_pushlstring(L, content.data(), content.size());
        return 1;
      }
    } else {
      lua_pushliteral(L, "");
      return 1;
    }
  }
  lua_pushnil(L);
  return 1;
}

int l_storage_writeFile(lua_State* L) {
  auto* ctx = getContext(L);
  const char* relPath = luaL_checkstring(L, 1);
  size_t len = 0;
  const char* data = luaL_checklstring(L, 2, &len);
  const std::string fullPath = ctx ? ctx->resolvePath(relPath) : std::string(relPath);

  HalFile file;
  if (Storage.openFileForWrite("LUA", fullPath, file)) {
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(data), len);
    lua_pushboolean(L, written == len);
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_storage_exists(lua_State* L) {
  auto* ctx = getContext(L);
  const char* relPath = luaL_checkstring(L, 1);
  const std::string fullPath = ctx ? ctx->resolvePath(relPath) : std::string(relPath);
  lua_pushboolean(L, Storage.exists(fullPath.c_str()));
  return 1;
}

int l_storage_remove(lua_State* L) {
  auto* ctx = getContext(L);
  const char* relPath = luaL_checkstring(L, 1);
  const std::string fullPath = ctx ? ctx->resolvePath(relPath) : std::string(relPath);
  lua_pushboolean(L, Storage.remove(fullPath.c_str()));
  return 1;
}

// ---------------------------------------------------------------------------
// Crosspoint / System API
// ---------------------------------------------------------------------------

int l_crosspoint_millis(lua_State* L) {
  lua_pushinteger(L, static_cast<lua_Integer>(millis()));
  return 1;
}

int l_crosspoint_requestUpdate(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->activity) {
    const bool immediate = lua_toboolean(L, 1);
    ctx->activity->requestUpdate(immediate);
  }
  return 0;
}

int l_crosspoint_finish(lua_State* L) {
  Activity::finish();
  return 0;
}

static void extractLogArgs(lua_State* L, const char*& tag, const char*& msg) {
  if (lua_gettop(L) >= 2) {
    tag = luaL_tolstring(L, 1, nullptr);
    msg = luaL_tolstring(L, 2, nullptr);
  } else if (lua_gettop(L) == 1) {
    tag = "LUA";
    msg = luaL_tolstring(L, 1, nullptr);
  } else {
    tag = "LUA";
    msg = "";
  }
}

int l_log_debug(lua_State* L) {
  const char *tag = nullptr, *msg = nullptr;
  extractLogArgs(L, tag, msg);
  LOG_DBG(tag, "%s", msg);
  return 0;
}

int l_log_info(lua_State* L) {
  const char *tag = nullptr, *msg = nullptr;
  extractLogArgs(L, tag, msg);
  LOG_INF(tag, "%s", msg);
  return 0;
}

int l_log_warn(lua_State* L) {
  const char *tag = nullptr, *msg = nullptr;
  extractLogArgs(L, tag, msg);
  LOG_INF(tag, "[WARN] %s", msg);
  return 0;
}

int l_log_error(lua_State* L) {
  const char *tag = nullptr, *msg = nullptr;
  extractLogArgs(L, tag, msg);
  LOG_ERR(tag, "%s", msg);
  return 0;
}

int l_crosspoint_log(lua_State* L) {
  return l_log_info(L);
}

int l_crosspoint_getMemoryInfo(lua_State* L) {
  lua_newtable(L);
  const int luaKb = lua_gc(L, LUA_GCCOUNT, 0);
  lua_pushinteger(L, luaKb);
  lua_setfield(L, -2, "luaMemoryKb");

#if defined(ESP32)
  lua_pushinteger(L, ESP.getFreeHeap() / 1024);
  lua_setfield(L, -2, "freeHeapKb");
  lua_pushinteger(L, ESP.getFreePsram() / 1024);
  lua_setfield(L, -2, "freePsramKb");
#else
  lua_pushinteger(L, 8192);
  lua_setfield(L, -2, "freeHeapKb");
  lua_pushinteger(L, 8192);
  lua_setfield(L, -2, "freePsramKb");
#endif
  return 1;
}

int l_crosspoint_getBattery(lua_State* L) {
  lua_newtable(L);
  lua_pushinteger(L, powerManager.getBatteryPercentage());
  lua_setfield(L, -2, "percentage");
  return 1;
}

int l_crosspoint_getTime(lua_State* L) {
  struct tm t = {};
  bool ok = halClock.localTime(t);
  if (!ok) {
    time_t now = time(nullptr);
    localtime_r(&now, &t);
  }
  lua_newtable(L);
  lua_pushinteger(L, t.tm_year + 1900);
  lua_setfield(L, -2, "year");
  lua_pushinteger(L, t.tm_mon + 1);
  lua_setfield(L, -2, "month");
  lua_pushinteger(L, t.tm_mday);
  lua_setfield(L, -2, "day");
  lua_pushinteger(L, t.tm_hour);
  lua_setfield(L, -2, "hour");
  lua_pushinteger(L, t.tm_min);
  lua_setfield(L, -2, "min");
  lua_pushinteger(L, t.tm_sec);
  lua_setfield(L, -2, "sec");
  return 1;
}

int appModuleSearcher(lua_State* L) {
  auto* ctx = getContext(L);
  const char* rawModName = luaL_checkstring(L, 1);
  std::string modPath = rawModName;
  for (char& c : modPath) {
    if (c == '.') c = '/';
  }

  std::vector<std::string> candidates;
  if (ctx) {
    candidates.push_back(ctx->resolvePath((modPath + ".lua").c_str()));
    candidates.push_back(ctx->resolvePath((modPath + "/init.lua").c_str()));
  } else {
    candidates.push_back(modPath + ".lua");
    candidates.push_back(modPath + "/init.lua");
  }

  std::string errorLog;
  for (const auto& path : candidates) {
    HalFile file;
    if (Storage.openFileForRead("LUA", path, file)) {
      const size_t sz = file.size();
      std::string content;
      content.resize(sz);
      if (sz > 0) {
        file.read(&content[0], sz);
      }
      if (luaL_loadbuffer(L, content.data(), content.size(), ("@" + path).c_str()) == LUA_OK) {
        return 1;
      }
      return lua_error(L);
    }
    errorLog += "\n\tno file '" + path + "'";
  }

  lua_pushstring(L, errorLog.c_str());
  return 1;
}

int l_crosspoint_isWifiConnected(lua_State* L) {
  lua_pushboolean(L, WiFi.status() == WL_CONNECTED);
  return 1;
}

int l_crosspoint_connectWifi(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->luaApp) {
    return luaL_error(L, "No LuaApp context available");
  }
  if (!lua_isfunction(L, 1)) {
    return luaL_error(L, "connectWifi requires a callback function(connected)");
  }
  lua_pushvalue(L, 1);
  const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ctx->luaApp->connectWifi(ref, false);
  return 0;
}

int l_crosspoint_withWifi(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->luaApp) {
    return luaL_error(L, "No LuaApp context available");
  }
  if (!lua_isfunction(L, 1)) {
    return luaL_error(L, "withWifi requires a callback function(connected)");
  }
  lua_pushvalue(L, 1);
  const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ctx->luaApp->connectWifi(ref, true);
  return 0;
}

int l_crosspoint_disconnectWifi(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->luaApp) {
    ctx->luaApp->disconnectWifi();
  }
#if defined(ESP32) || defined(ARDUINO)
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
  }
#endif
  return 0;
}

int l_crosspoint_httpGet(lua_State* L) {
  const char* url = luaL_checkstring(L, 1);
  std::string response;
  if (HttpDownloader::fetchUrl(url, response)) {
    lua_pushlstring(L, response.data(), response.size());
    return 1;
  }
  lua_pushnil(L);
  return 1;
}

int l_crosspoint_setSleepApp(lua_State* L) {
  const char* id = luaL_checkstring(L, 1);
  auto& settings = CrossPointSettings::getInstance();
  settings.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::APP;
  strncpy(settings.sleepScreenAppId, id, sizeof(settings.sleepScreenAppId) - 1);
  settings.sleepScreenAppId[sizeof(settings.sleepScreenAppId) - 1] = '\0';
  settings.saveToFile();
  lua_pushboolean(L, true);
  return 1;
}

int l_crosspoint_getSleepApp(lua_State* L) {
  const auto& settings = CrossPointSettings::getInstance();
  if (settings.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::APP) {
    lua_pushstring(L, settings.sleepScreenAppId);
  } else {
    lua_pushstring(L, "");
  }
  return 1;
}

int l_crosspoint_clearSleepApp(lua_State* L) {
  auto& settings = CrossPointSettings::getInstance();
  settings.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::DARK;
  settings.sleepScreenAppId[0] = '\0';
  settings.saveToFile();
  lua_pushboolean(L, true);
  return 1;
}

void registerModule(lua_State* L, const char* name, const luaL_Reg* funcs, HostContext* ctx) {
  lua_newtable(L);
  for (; funcs->name != nullptr; funcs++) {
    lua_pushstring(L, funcs->name);
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, funcs->func, 1);
    lua_settable(L, -3);
  }
  lua_setglobal(L, name);
}

}  // namespace

void registerBindings(lua_State* L, HostContext* ctx) {
  // Register gfx functions
  static const luaL_Reg gfxFuncs[] = {
      {"getWidth", l_gfx_getWidth},
      {"getHeight", l_gfx_getHeight},
      {"clearScreen", l_gfx_clearScreen},
      {"drawPixel", l_gfx_drawPixel},
      {"drawLine", l_gfx_drawLine},
      {"drawRect", l_gfx_drawRect},
      {"fillRect", l_gfx_fillRect},
      {"fillRectDither", l_gfx_fillRectDither},
      {"drawRoundedRect", l_gfx_drawRoundedRect},
      {"fillRoundedRect", l_gfx_fillRoundedRect},
      {"drawCircle", l_gfx_drawCircle},
      {"drawText", l_gfx_drawText},
      {"drawCenteredText", l_gfx_drawCenteredText},
      {"getTextWidth", l_gfx_getTextWidth},
      {"getLineHeight", l_gfx_getLineHeight},
      {"drawBitmapFile", l_gfx_drawBitmapFile},
      {"drawSprite", l_gfx_drawSprite},
      {"drawQrCode", l_gfx_drawQrCode},
      {"displayBuffer", l_gfx_displayBuffer},
      {"setOrientation", l_gfx_setOrientation},
      {"getOrientation", l_gfx_getOrientation},
      {nullptr, nullptr},
  };
  registerModule(L, "gfx", gfxFuncs, ctx);

  // Set gfx constants
  lua_getglobal(L, "gfx");
  lua_pushinteger(L, static_cast<int>(GfxRenderer::Orientation::Portrait));
  lua_setfield(L, -2, "ORIENTATION_PORTRAIT");
  lua_pushinteger(L, static_cast<int>(GfxRenderer::Orientation::LandscapeClockwise));
  lua_setfield(L, -2, "ORIENTATION_LANDSCAPE");
  lua_pushinteger(L, static_cast<int>(GfxRenderer::Orientation::PortraitInverted));
  lua_setfield(L, -2, "ORIENTATION_PORTRAIT_INVERTED");
  lua_pushinteger(L, static_cast<int>(GfxRenderer::Orientation::LandscapeCounterClockwise));
  lua_setfield(L, -2, "ORIENTATION_LANDSCAPE_CCW");
  lua_pushinteger(L, UI_10_FONT_ID);
  lua_setfield(L, -2, "FONT_UI_10");
  lua_pushinteger(L, UI_12_FONT_ID);
  lua_setfield(L, -2, "FONT_UI_12");
  lua_pushinteger(L, SMALL_FONT_ID);
  lua_setfield(L, -2, "FONT_SMALL");
  lua_pushinteger(L, NOTOSANS_12_FONT_ID);
  lua_setfield(L, -2, "FONT_NOTOSANS_12");
  lua_pushinteger(L, NOTOSANS_14_FONT_ID);
  lua_setfield(L, -2, "FONT_NOTOSANS_14");
  lua_pushinteger(L, NOTOSANS_16_FONT_ID);
  lua_setfield(L, -2, "FONT_NOTOSANS_16");
  lua_pushinteger(L, NOTOSERIF_12_FONT_ID);
  lua_setfield(L, -2, "FONT_NOTOSERIF_12");
  lua_pushinteger(L, NOTOSERIF_14_FONT_ID);
  lua_setfield(L, -2, "FONT_NOTOSERIF_14");

  lua_pushinteger(L, 0);
  lua_setfield(L, -2, "REFRESH_FAST");
  lua_pushinteger(L, 1);
  lua_setfield(L, -2, "REFRESH_HALF");
  lua_pushinteger(L, 2);
  lua_setfield(L, -2, "REFRESH_FULL");

  lua_pushinteger(L, static_cast<int>(Color::Black));
  lua_setfield(L, -2, "COLOR_BLACK");
  lua_pushinteger(L, static_cast<int>(Color::DarkGray));
  lua_setfield(L, -2, "COLOR_DARK_GRAY");
  lua_pushinteger(L, static_cast<int>(Color::LightGray));
  lua_setfield(L, -2, "COLOR_LIGHT_GRAY");
  lua_pushinteger(L, static_cast<int>(Color::White));
  lua_setfield(L, -2, "COLOR_WHITE");
  lua_pop(L, 1);

  // Register input functions
  static const luaL_Reg inputFuncs[] = {
      {"wasPressed", l_input_wasPressed},
      {"isPressed", l_input_isPressed},
      {"wasScreenTapped", l_input_wasScreenTapped},
      {"isTouchDown", l_input_isTouchDown},
      {"getTouch", l_input_getTouch},
      {"wasTouchDown", l_input_wasTouchDown},
      {"wasTouchReleased", l_input_wasTouchReleased},
      {nullptr, nullptr},
  };
  registerModule(L, "input", inputFuncs, ctx);

  // Set input button constants
  lua_getglobal(L, "input");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::Back));
  lua_setfield(L, -2, "BTN_BACK");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::Confirm));
  lua_setfield(L, -2, "BTN_CONFIRM");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::Left));
  lua_setfield(L, -2, "BTN_LEFT");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::Right));
  lua_setfield(L, -2, "BTN_RIGHT");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::Up));
  lua_setfield(L, -2, "BTN_UP");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::Down));
  lua_setfield(L, -2, "BTN_DOWN");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::PageBack));
  lua_setfield(L, -2, "BTN_PAGE_BACK");
  lua_pushinteger(L, static_cast<int>(MappedInputManager::Button::PageForward));
  lua_setfield(L, -2, "BTN_PAGE_FORWARD");
  lua_pop(L, 1);

  // Register log module
  static const luaL_Reg logFuncs[] = {
      {"debug", l_log_debug},
      {"info", l_log_info},
      {"warn", l_log_warn},
      {"error", l_log_error},
      {nullptr, nullptr},
  };
  registerModule(L, "log", logFuncs, ctx);

  // Register storage functions
  static const luaL_Reg storageFuncs[] = {
      {"readFile", l_storage_readFile},
      {"writeFile", l_storage_writeFile},
      {"exists", l_storage_exists},
      {"remove", l_storage_remove},
      {nullptr, nullptr},
  };
  registerModule(L, "storage", storageFuncs, ctx);

  // Register crosspoint / system functions
  static const luaL_Reg crosspointFuncs[] = {
      {"millis", l_crosspoint_millis},
      {"requestUpdate", l_crosspoint_requestUpdate},
      {"finish", l_crosspoint_finish},
      {"log", l_crosspoint_log},
      {"getMemoryInfo", l_crosspoint_getMemoryInfo},
      {"isWifiConnected", l_crosspoint_isWifiConnected},
      {"connectWifi", l_crosspoint_connectWifi},
      {"withWifi", l_crosspoint_withWifi},
      {"disconnectWifi", l_crosspoint_disconnectWifi},
      {"httpGet", l_crosspoint_httpGet},
      {"getBattery", l_crosspoint_getBattery},
      {"getTime", l_crosspoint_getTime},
      {"setSleepApp", l_crosspoint_setSleepApp},
      {"getSleepApp", l_crosspoint_getSleepApp},
      {"clearSleepApp", l_crosspoint_clearSleepApp},
      {nullptr, nullptr},
  };
  registerModule(L, "crosspoint", crosspointFuncs, ctx);

  // Register custom package searcher for modular require(...)
  lua_getglobal(L, "package");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "searchers");
    if (lua_istable(L, -1)) {
      const int count = static_cast<int>(lua_rawlen(L, -1));
      for (int i = count; i >= 2; --i) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 1);
      }
      lua_pushlightuserdata(L, ctx);
      lua_pushcclosure(L, appModuleSearcher, 1);
      lua_rawseti(L, -2, 2);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
}

}  // namespace lua_host
