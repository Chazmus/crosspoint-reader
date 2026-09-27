#include "LuaBindings.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include "LuaAppActivity.h"
#include "LuaUiHost.h"
#include <FreeInkUI.h>
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

// ---------------------------------------------------------------------------
// UI API (Phase 1: Core Navigation & App Kit)
// ---------------------------------------------------------------------------

static int l_ui_getTheme(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost) {
    lua_newtable(L);
    return 1;
  }
  const auto& tokens = ctx->uiHost->getTokens();
  lua_newtable(L);
  lua_pushstring(L, ctx->uiHost->getThemeName().c_str());
  lua_setfield(L, -2, "name");
  lua_pushinteger(L, tokens.controlRadius);
  lua_setfield(L, -2, "controlRadius");
  lua_pushinteger(L, tokens.listRowRadius);
  lua_setfield(L, -2, "listRowRadius");
  lua_pushinteger(L, tokens.headerHeight);
  lua_setfield(L, -2, "headerHeight");
  lua_pushinteger(L, tokens.headerUnderline);
  lua_setfield(L, -2, "headerUnderline");
  lua_pushinteger(L, 36);
  lua_setfield(L, -2, "tabBarHeight");
  lua_pushinteger(L, NOTOSANS_14_FONT_ID);
  lua_setfield(L, -2, "fontTitle");
  lua_pushinteger(L, NOTOSERIF_12_FONT_ID);
  lua_setfield(L, -2, "fontBody");
  lua_pushinteger(L, UI_10_FONT_ID);
  lua_setfield(L, -2, "fontSmall");
  lua_pushboolean(L, true);
  lua_setfield(L, -2, "isTouch");
  return 1;
}

static int l_ui_setTheme(lua_State* L) {
  auto* ctx = getContext(L);
  if (ctx && ctx->uiHost && lua_isstring(L, 1)) {
    ctx->uiHost->setThemeByName(lua_tostring(L, 1));
    if (ctx->activity) ctx->activity->requestUpdate();
  }
  return 0;
}

static int l_ui_drawHeader(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();
  const int screenW = ctx->renderer ? ctx->renderer->getScreenWidth() : 480;
  const int screenH = ctx->renderer ? ctx->renderer->getScreenHeight() : 800;

  std::string title;
  std::string subtitle;
  std::string rightLabel;
  bool showBack = false;
  bool showBattery = false;
  bool showClock = false;
  int headerH = tokens.headerHeight > 0 ? tokens.headerHeight : 44;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "title");
    if (lua_isstring(L, -1)) title = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "subtitle");
    if (lua_isstring(L, -1)) subtitle = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "rightLabel");
    if (!lua_isstring(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "trailingLabel");
    }
    if (lua_isstring(L, -1)) rightLabel = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "showBack");
    if (lua_isboolean(L, -1)) showBack = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "showBattery");
    if (lua_isboolean(L, -1)) showBattery = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "showClock");
    if (lua_isboolean(L, -1)) showClock = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "height");
    if (lua_isnumber(L, -1)) headerH = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  freeink::ui::HeaderProps hp;
  hp.title = title.c_str();
  if (!subtitle.empty()) hp.subtitle = subtitle.c_str();
  if (!rightLabel.empty()) hp.trailingLabel = rightLabel.c_str();
  hp.titleText.font = freeink::ui::GfxRendererTarget::FONT_TITLE;
  hp.subtitleText.font = freeink::ui::GfxRendererTarget::FONT_SMALL;
  hp.sidePadding = tokens.headerSidePadding;
  hp.centered = (tokens.headerTitleAlign == freeink::ui::TextAlign::Center);
  hp.borderEdges = tokens.headerUnderline > 0 ? freeink::ui::EdgeBottom : freeink::ui::EdgesNone;

  if (showBack) {
    hp.leadingIcon = lua_host::LuaUiHost::getHeaderBackIcon();
    lua_getfield(L, 1, "onBack");
    if (lua_isfunction(L, -1)) {
      hp.leadingAction = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "back");
    } else {
      hp.leadingAction = ctx->uiHost->registerCallback(L, 0, 0, "back");
    }
    lua_pop(L, 1);
  }

  lua_getfield(L, 1, "trailingAction");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "onClick");
    if (lua_isfunction(L, -1)) {
      hp.trailingAction = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "trailing");
    }
    lua_pop(L, 1);
  } else {
    lua_pop(L, 1);
    lua_getfield(L, 1, "onTrailing");
    if (lua_isfunction(L, -1)) {
      hp.trailingAction = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "trailing");
    }
    lua_pop(L, 1);
  }

  std::string clockStr;
  if (showClock) {
    char buf[16];
    struct tm t = {};
    if (halClock.localTime(t)) {
      snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
      clockStr = buf;
      hp.status.clockText = clockStr.c_str();
      hp.status.clockCentered = false;
    }
  }
  if (showBattery) {
    hp.status.showBattery = true;
    hp.status.battery.glyphWidth = 24;
    hp.status.battery.glyphHeight = 12;
    hp.status.battery.percent = static_cast<uint8_t>(powerManager.getBatteryPercentage());
  }
  hp.status.stripHeight = headerH;

  freeink::ui::Rect headerRect{0, 0, static_cast<int16_t>(screenW), static_cast<int16_t>(headerH)};
  freeink::ui::header(*frame, headerRect, hp);

  lua_newtable(L);
  lua_pushinteger(L, 0);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, headerH);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, screenW);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, screenH - headerH);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawTabBar(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();
  const int screenW = ctx->renderer ? ctx->renderer->getScreenWidth() : 480;

  int x = 0;
  int y = 48;
  int w = screenW;
  int h = 36;
  int selectedIdx = 1;
  std::vector<std::string> tabLabels;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "selectedIndex");
    if (!lua_isnumber(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "selected");
    }
    if (lua_isnumber(L, -1)) selectedIdx = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "tabs");
    if (lua_istable(L, -1)) {
      const int count = static_cast<int>(lua_rawlen(L, -1));
      for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isstring(L, -1)) {
          tabLabels.push_back(lua_tostring(L, -1));
        } else if (lua_istable(L, -1)) {
          lua_getfield(L, -1, "label");
          tabLabels.push_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  freeink::ui::ActionId barAction = freeink::ui::NO_ACTION;
  lua_getfield(L, 1, "onSelect");
  if (lua_isfunction(L, -1)) {
    barAction = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "tabBar");
  }
  lua_pop(L, 1);

  std::vector<freeink::ui::TabItem> items;
  for (size_t i = 0; i < tabLabels.size(); ++i) {
    freeink::ui::TabItem it;
    it.label = tabLabels[i].c_str();
    it.value = static_cast<int16_t>(i + 1);
    it.selected = (selectedIdx == static_cast<int>(i + 1));
    it.enabled = true;
    items.push_back(it);
  }

  freeink::ui::TabBarProps tp;
  tp.tabs = items.data();
  tp.count = static_cast<uint8_t>(items.size());
  tp.action = barAction;
  tp.text.font = freeink::ui::GfxRendererTarget::FONT_SMALL;
  if (ctx->uiHost->isTabPillFullSlot()) {
    tp.tabStyles.selected.background = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    tp.tabStyles.selected.foreground = freeink::ui::Paint::solid(freeink::ui::Color::White);
    tp.tabStyles.selected.radius = tokens.controlRadius;
  }

  freeink::ui::Rect barRect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::tabBar(*frame, barRect, tp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawButton(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 120, h = 44;
  std::string label;
  std::string variant = "secondary";
  bool enabled = true;
  int radius = tokens.controlRadius;

  int tableIdx = 0;
  if (lua_istable(L, 1)) {
    tableIdx = 1;
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "label");
    if (!lua_isstring(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "text");
    }
    if (lua_isstring(L, -1)) label = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "variant");
    if (lua_isstring(L, -1)) variant = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  } else {
    x = checkInt(L, 1);
    y = checkInt(L, 2);
    w = checkInt(L, 3);
    h = checkInt(L, 4);
    if (lua_isstring(L, 5)) label = lua_tostring(L, 5);
    if (lua_istable(L, 6)) {
      tableIdx = 6;
      lua_getfield(L, 6, "variant");
      if (lua_isstring(L, -1)) variant = lua_tostring(L, -1);
      lua_pop(L, 1);
      lua_getfield(L, 6, "enabled");
      if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
  }

  freeink::ui::ActionId btnAction = freeink::ui::NO_ACTION;
  if (tableIdx > 0) {
    lua_getfield(L, tableIdx, "onClick");
    if (lua_isfunction(L, -1)) {
      btnAction = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "button");
    }
    lua_pop(L, 1);
  }

  freeink::ui::ButtonProps bp;
  bp.label = label.c_str();
  bp.action = btnAction;
  bp.enabled = enabled;
  bp.radius = static_cast<uint8_t>(radius);
  bp.text.font = freeink::ui::GfxRendererTarget::FONT_BODY;

  if (variant == "primary") {
    bp.styles.normal.background = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    bp.styles.normal.foreground = freeink::ui::Paint::solid(freeink::ui::Color::White);
    bp.styles.normal.border = freeink::ui::Paint::none();
  } else if (variant == "ghost" || variant == "flat") {
    bp.styles.normal.background = freeink::ui::Paint::none();
    bp.styles.normal.foreground = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    bp.styles.normal.border = freeink::ui::Paint::none();
  } else {
    bp.styles.normal.background = freeink::ui::Paint::solid(freeink::ui::Color::White);
    bp.styles.normal.foreground = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    bp.styles.normal.border = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    bp.styles.normal.borderWidth = 1;
  }

  freeink::ui::Rect btnRect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::button(*frame, btnRect, bp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawCard(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 200, h = 100;
  int radius = tokens.listRowRadius > 0 ? tokens.listRowRadius : tokens.controlRadius;
  int padding = 12;
  std::string variant = "outlined";

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "padding");
    if (lua_isnumber(L, -1)) padding = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "variant");
    if (lua_isstring(L, -1)) variant = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  freeink::ui::Rect cardRect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};

  lua_getfield(L, 1, "onClick");
  if (lua_isfunction(L, -1)) {
    freeink::ui::ActionId act = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "card");
    frame->hit(cardRect, act);
  }
  lua_pop(L, 1);

  if (variant == "filled") {
    frame->target().fill(cardRect, freeink::ui::Paint::solid(freeink::ui::Color::Black), static_cast<uint8_t>(radius));
  } else if (variant == "dithered") {
    frame->target().fill(cardRect, freeink::ui::Paint::dither(freeink::ui::Color::LightGray), static_cast<uint8_t>(radius));
    frame->target().stroke(cardRect, freeink::ui::Paint::solid(freeink::ui::Color::Black), 1, static_cast<uint8_t>(radius));
  } else {
    frame->target().fill(cardRect, freeink::ui::Paint::solid(freeink::ui::Color::White), static_cast<uint8_t>(radius));
    frame->target().stroke(cardRect, freeink::ui::Paint::solid(freeink::ui::Color::Black), 1, static_cast<uint8_t>(radius));
  }

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");

  lua_pushinteger(L, x + padding);
  lua_setfield(L, -2, "innerX");
  lua_pushinteger(L, y + padding);
  lua_setfield(L, -2, "innerY");
  lua_pushinteger(L, w - padding * 2);
  lua_setfield(L, -2, "innerW");
  lua_pushinteger(L, h - padding * 2);
  lua_setfield(L, -2, "innerH");
  return 1;
}

static int l_ui_drawBadge(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->renderer) {
    lua_newtable(L);
    return 1;
  }

  int x = 0, y = 0;
  std::string text;
  std::string variant = "filled";
  int fontId = UI_10_FONT_ID;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "text");
    if (!lua_isstring(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "label");
    }
    if (lua_isstring(L, -1)) text = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "variant");
    if (lua_isstring(L, -1)) variant = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "font");
    if (lua_isnumber(L, -1)) fontId = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  int tw = ctx->renderer->getTextWidth(fontId, text.c_str());
  int th = ctx->renderer->getLineHeight(fontId);
  const int hPad = 7;
  const int vPad = 3;
  int w = tw + hPad * 2;
  int h = th + vPad * 2;
  int radius = h / 2;

  if (variant == "filled") {
    ctx->renderer->fillRoundedRect(x, y, w, h, radius, Color::Black);
    ctx->renderer->drawText(fontId, x + hPad, y + vPad, text.c_str(), false);
  } else {
    ctx->renderer->fillRoundedRect(x, y, w, h, radius, Color::White);
    ctx->renderer->drawRoundedRect(x, y, w, h, 1, radius, true);
    ctx->renderer->drawText(fontId, x + hPad, y + vPad, text.c_str(), true);
  }

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawToggle(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();

  int x = 0, y = 0, w = 38, h = 20;
  bool checked = false;
  bool enabled = true;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "checked");
    if (lua_isboolean(L, -1)) checked = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  freeink::ui::ActionId toggleAction = freeink::ui::NO_ACTION;
  lua_getfield(L, 1, "onToggle");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    lua_getfield(L, 1, "onClick");
  }
  if (lua_isfunction(L, -1)) {
    toggleAction = ctx->uiHost->registerCallback(L, lua_gettop(L), checked ? 0 : 1, "toggle");
  }
  lua_pop(L, 1);

  freeink::ui::ToggleProps tp;
  tp.checked = checked;
  tp.enabled = enabled;
  tp.action = toggleAction;
  tp.width = static_cast<int16_t>(w);
  tp.height = static_cast<int16_t>(h);

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::toggle(*frame, rect, tp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawDialog(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const int screenW = ctx->renderer ? ctx->renderer->getScreenWidth() : 480;
  const int screenH = ctx->renderer ? ctx->renderer->getScreenHeight() : 800;

  std::string title;
  std::string headline;
  std::string message;
  bool dimBackground = true;
  std::vector<freeink::ui::DialogOption> options;
  std::vector<std::string> optLabels;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "title");
    if (lua_isstring(L, -1)) title = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "headline");
    if (lua_isstring(L, -1)) headline = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "message");
    if (lua_isstring(L, -1)) message = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "dimBackground");
    if (lua_isboolean(L, -1)) dimBackground = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "buttons");
    if (lua_istable(L, -1)) {
      const int count = static_cast<int>(lua_rawlen(L, -1));
      for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_istable(L, -1)) {
          lua_getfield(L, -1, "label");
          std::string l = lua_isstring(L, -1) ? lua_tostring(L, -1) : "OK";
          lua_pop(L, 1);
          optLabels.push_back(l);

          freeink::ui::ActionId act = freeink::ui::NO_ACTION;
          lua_getfield(L, -1, "onClick");
          if (lua_isfunction(L, -1)) {
            act = ctx->uiHost->registerCallback(L, lua_gettop(L), static_cast<int16_t>(i), "dialogButton");
          }
          lua_pop(L, 1);

          freeink::ui::DialogOption opt;
          opt.label = optLabels.back().c_str();
          opt.action = act;
          opt.value = static_cast<int16_t>(i);
          options.push_back(opt);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  if (options.empty()) {
    optLabels.push_back("OK");
    freeink::ui::DialogOption opt;
    opt.label = optLabels.back().c_str();
    opt.action = ctx->uiHost->registerCallback(L, 0, 0, "dialogClose");
    options.push_back(opt);
  }

  freeink::ui::OptionDialogProps dp;
  dp.title = title.empty() ? nullptr : title.c_str();
  dp.headline = headline.empty() ? nullptr : headline.c_str();
  dp.message = message.empty() ? nullptr : message.c_str();
  dp.options = options.data();
  dp.optionCount = static_cast<uint8_t>(options.size());
  dp.dimBackground = dimBackground;
  dp.titleText.font = freeink::ui::GfxRendererTarget::FONT_TITLE;
  dp.messageText.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  dp.buttonText.font = freeink::ui::GfxRendererTarget::FONT_BODY;

  const int dialogW = std::min(400, screenW - 40);
  const int dialogH = freeink::ui::optionDialogHeight(frame->target(), dp, static_cast<int16_t>(dialogW));
  const int dx = (screenW - dialogW) / 2;
  const int dy = (screenH - dialogH) / 2;
  freeink::ui::Rect drect{static_cast<int16_t>(dx), static_cast<int16_t>(dy), static_cast<int16_t>(dialogW), static_cast<int16_t>(dialogH)};

  freeink::ui::optionDialog(*frame, drect, dp);

  lua_newtable(L);
  lua_pushinteger(L, dx);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, dy);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, dialogW);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, dialogH);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawToast(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    return 0;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const int screenW = ctx->renderer ? ctx->renderer->getScreenWidth() : 480;
  const int screenH = ctx->renderer ? ctx->renderer->getScreenHeight() : 800;

  std::string msg;
  std::string anchor = "bottom";

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "message");
    if (!lua_isstring(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "text");
    }
    if (lua_isstring(L, -1)) msg = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "anchor");
    if (lua_isstring(L, -1)) anchor = lua_tostring(L, -1);
    lua_pop(L, 1);
  } else if (lua_isstring(L, 1)) {
    msg = lua_tostring(L, 1);
  }

  freeink::ui::ToastProps tp;
  tp.message = msg.c_str();
  tp.text.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  if (anchor == "top") {
    tp.anchor = freeink::ui::ToastAnchor::Top;
  } else if (anchor == "center") {
    tp.anchor = freeink::ui::ToastAnchor::Center;
  } else {
    tp.anchor = freeink::ui::ToastAnchor::Bottom;
  }

  freeink::ui::Rect trect{0, 0, static_cast<int16_t>(screenW), static_cast<int16_t>(screenH)};
  freeink::ui::toast(*frame, trect, tp);
  return 0;
}

static int l_ui_drawSlider(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 200, h = 32;
  int value = 0;
  int minVal = 0;
  int maxVal = 100;
  bool enabled = true;
  int radius = tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "value");
    if (lua_isnumber(L, -1)) value = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "min");
    if (lua_isnumber(L, -1)) minVal = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "max");
    if (lua_isnumber(L, -1)) maxVal = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  if (maxVal <= minVal) maxVal = minVal + 1;
  if (value < minVal) value = minVal;
  if (value > maxVal) value = maxVal;

  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onChange");
    if (!lua_isfunction(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "onClick");
    }
    if (lua_isfunction(L, -1)) {
      int pad = 8;
      auto invoker = [x, w, minVal, maxVal, pad](lua_State* state, int touchX, int /*touchY*/, int16_t /*val*/) -> int {
        int trackW = w - 2 * pad;
        if (trackW <= 0) trackW = 1;
        int relX = touchX - (x + pad);
        if (relX < 0) relX = 0;
        if (relX > trackW) relX = trackW;
        int newVal = minVal + static_cast<int>((static_cast<int64_t>(relX) * (maxVal - minVal)) / trackW);
        lua_pushinteger(state, newVal);
        return 1;
      };
      act = ctx->uiHost->registerCallbackWithInvoker(L, lua_gettop(L), std::move(invoker), 0, "slider");
    }
    lua_pop(L, 1);
  }

  freeink::ui::SliderProps sp;
  sp.value = value - minVal;
  sp.max = maxVal - minVal;
  sp.action = act;
  sp.enabled = enabled;
  sp.radius = static_cast<uint8_t>(radius);
  sp.horizontalPadding = 8;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::slider(*frame, rect, sp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawCapsuleSlider(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 200, h = 36;
  int value = 0;
  int minVal = 0;
  int maxVal = 100;
  bool enabled = true;
  int radius = tokens.capsuleRadius > 0 ? tokens.capsuleRadius : tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "value");
    if (lua_isnumber(L, -1)) value = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "min");
    if (lua_isnumber(L, -1)) minVal = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "max");
    if (lua_isnumber(L, -1)) maxVal = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  if (maxVal <= minVal) maxVal = minVal + 1;
  if (value < minVal) value = minVal;
  if (value > maxVal) value = maxVal;

  const int stroke = 2;
  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onChange");
    if (!lua_isfunction(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "onClick");
    }
    if (lua_isfunction(L, -1)) {
      auto invoker = [x, w, h, minVal, maxVal, stroke](lua_State* state, int touchX, int /*touchY*/, int16_t /*val*/) -> int {
        int innerH = h - 2 * stroke;
        int cap = innerH / 2;
        int travel = (w - 2 * stroke) - 2 * cap;
        if (travel <= 0) travel = 1;
        int relX = touchX - (x + stroke + cap);
        if (relX < 0) relX = 0;
        if (relX > travel) relX = travel;
        int newVal = minVal + static_cast<int>((static_cast<int64_t>(relX) * (maxVal - minVal)) / travel);
        lua_pushinteger(state, newVal);
        return 1;
      };
      act = ctx->uiHost->registerCallbackWithInvoker(L, lua_gettop(L), std::move(invoker), 0, "capsuleSlider");
    }
    lua_pop(L, 1);
  }

  freeink::ui::CapsuleSliderProps cp;
  cp.value = value - minVal;
  cp.max = maxVal - minVal;
  cp.action = act;
  cp.stroke = stroke;
  cp.radius = static_cast<uint8_t>(radius);
  cp.enabled = enabled;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::capsuleSlider(*frame, rect, cp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawProgressBar(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 200, h = 12;
  int value = 0;
  int maxVal = 100;
  int radius = tokens.controlRadius;
  std::string variant = "solid";

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "value");
    if (lua_isnumber(L, -1)) value = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "max");
    if (lua_isnumber(L, -1)) maxVal = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "variant");
    if (lua_isstring(L, -1)) variant = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  freeink::ui::ProgressBarProps pbp;
  pbp.value = value;
  pbp.max = maxVal;
  pbp.radius = static_cast<uint8_t>(radius);
  pbp.border = freeink::ui::Paint::solid(freeink::ui::Color::Black);
  pbp.borderWidth = 1;
  pbp.track = freeink::ui::Paint::solid(freeink::ui::Color::White);
  if (variant == "dither") {
    pbp.fill = freeink::ui::Paint::dither(freeink::ui::Color::DarkGray);
  } else {
    pbp.fill = freeink::ui::Paint::solid(freeink::ui::Color::Black);
  }

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::progressBar(*frame, rect, pbp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawCheckbox(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 160, h = 36;
  std::string label;
  bool checked = false;
  bool enabled = true;
  int radius = tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "label");
    if (lua_isstring(L, -1)) label = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "checked");
    if (lua_isboolean(L, -1)) checked = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onToggle");
    if (!lua_isfunction(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "onClick");
    }
    if (lua_isfunction(L, -1)) {
      auto invoker = [checked](lua_State* state, int /*x*/, int /*y*/, int16_t /*val*/) -> int {
        lua_pushboolean(state, !checked);
        return 1;
      };
      act = ctx->uiHost->registerCallbackWithInvoker(L, lua_gettop(L), std::move(invoker), checked ? 0 : 1, "checkbox");
    }
    lua_pop(L, 1);
  }

  freeink::ui::CheckboxProps cp;
  cp.label = label.empty() ? nullptr : label.c_str();
  cp.checked = checked;
  cp.enabled = enabled;
  cp.action = act;
  cp.radius = static_cast<uint8_t>(radius);
  cp.text.font = freeink::ui::GfxRendererTarget::FONT_BODY;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::checkbox(*frame, rect, cp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawSettingRow(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 300, h = 48;
  std::string label;
  std::string subtitle;
  std::string value;
  bool drawChevron = false;
  bool enabled = true;
  int radius = tokens.listRowRadius > 0 ? tokens.listRowRadius : tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "label");
    if (lua_isstring(L, -1)) label = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "subtitle");
    if (lua_isstring(L, -1)) subtitle = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "value");
    if (lua_isstring(L, -1)) value = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "drawChevron");
    if (lua_isboolean(L, -1)) drawChevron = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onClick");
    if (lua_isfunction(L, -1)) {
      act = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "settingRow");
    }
    lua_pop(L, 1);
  }

  freeink::ui::SettingRowProps srp;
  srp.label = label.empty() ? nullptr : label.c_str();
  srp.subtitle = subtitle.empty() ? nullptr : subtitle.c_str();
  srp.value = value.empty() ? nullptr : value.c_str();
  srp.drawChevron = drawChevron;
  srp.enabled = enabled;
  srp.action = act;
  srp.radius = static_cast<uint8_t>(radius);
  srp.labelText.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  srp.subtitleText.font = freeink::ui::GfxRendererTarget::FONT_SMALL;
  srp.valueText.font = freeink::ui::GfxRendererTarget::FONT_BODY;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::settingRow(*frame, rect, srp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawToggleRow(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 300, h = 48;
  std::string label;
  std::string subtitle;
  bool checked = false;
  bool enabled = true;
  int radius = tokens.listRowRadius > 0 ? tokens.listRowRadius : tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "label");
    if (lua_isstring(L, -1)) label = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "subtitle");
    if (lua_isstring(L, -1)) subtitle = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "checked");
    if (lua_isboolean(L, -1)) checked = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onToggle");
    if (!lua_isfunction(L, -1)) {
      lua_pop(L, 1);
      lua_getfield(L, 1, "onClick");
    }
    if (lua_isfunction(L, -1)) {
      auto invoker = [checked](lua_State* state, int /*x*/, int /*y*/, int16_t /*val*/) -> int {
        lua_pushboolean(state, !checked);
        return 1;
      };
      act = ctx->uiHost->registerCallbackWithInvoker(L, lua_gettop(L), std::move(invoker), checked ? 0 : 1, "toggleRow");
    }
    lua_pop(L, 1);
  }

  freeink::ui::ToggleRowProps trp;
  trp.row.label = label.empty() ? nullptr : label.c_str();
  trp.row.subtitle = subtitle.empty() ? nullptr : subtitle.c_str();
  trp.row.enabled = enabled;
  trp.row.radius = static_cast<uint8_t>(radius);
  trp.row.labelText.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  trp.row.subtitleText.font = freeink::ui::GfxRendererTarget::FONT_SMALL;
  trp.checked = checked;
  trp.toggleAction = act;
  trp.radius = static_cast<uint8_t>(tokens.controlRadius);

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::toggleRow(*frame, rect, trp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawStepperRow(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 300, h = 48;
  std::string label;
  std::string subtitle;
  std::string value;
  bool enabled = true;
  int radius = tokens.listRowRadius > 0 ? tokens.listRowRadius : tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "label");
    if (lua_isstring(L, -1)) label = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "subtitle");
    if (lua_isstring(L, -1)) subtitle = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "value");
    if (lua_isstring(L, -1)) {
      value = lua_tostring(L, -1);
    } else if (lua_isnumber(L, -1)) {
      value = std::to_string(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);
    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  freeink::ui::ActionId decAct = freeink::ui::NO_ACTION;
  freeink::ui::ActionId incAct = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onDecrement");
    if (lua_isfunction(L, -1)) {
      decAct = ctx->uiHost->registerCallback(L, lua_gettop(L), -1, "stepperDec");
    }
    lua_pop(L, 1);
    lua_getfield(L, 1, "onIncrement");
    if (lua_isfunction(L, -1)) {
      incAct = ctx->uiHost->registerCallback(L, lua_gettop(L), 1, "stepperInc");
    }
    lua_pop(L, 1);
  }

  freeink::ui::StepperRowProps srp;
  srp.row.label = label.empty() ? nullptr : label.c_str();
  srp.row.subtitle = subtitle.empty() ? nullptr : subtitle.c_str();
  srp.row.enabled = enabled;
  srp.row.radius = static_cast<uint8_t>(radius);
  srp.row.labelText.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  srp.row.subtitleText.font = freeink::ui::GfxRendererTarget::FONT_SMALL;
  srp.row.valueText.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  srp.value = value.c_str();
  srp.decrement = decAct;
  srp.increment = incAct;
  srp.buttonRadius = static_cast<uint8_t>(tokens.controlRadius);

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::stepperRow(*frame, rect, srp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawRadioGroup(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 300, h = 40;
  int selectedValue = 1;
  int gap = 4;
  std::vector<std::string> labels;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "selectedIndex");
    if (lua_isnumber(L, -1)) selectedValue = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "gap");
    if (lua_isnumber(L, -1)) gap = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "options");
    if (lua_istable(L, -1)) {
      const int count = static_cast<int>(lua_rawlen(L, -1));
      for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isstring(L, -1)) {
          labels.push_back(lua_tostring(L, -1));
        } else if (lua_istable(L, -1)) {
          lua_getfield(L, -1, "label");
          labels.push_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onSelect");
    if (lua_isfunction(L, -1)) {
      act = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "radioGroup");
    }
    lua_pop(L, 1);
  }

  std::vector<freeink::ui::RadioOption> opts;
  for (size_t i = 0; i < labels.size(); ++i) {
    freeink::ui::RadioOption ro;
    ro.label = labels[i].c_str();
    ro.value = static_cast<int16_t>(i + 1);
    ro.enabled = true;
    opts.push_back(ro);
  }

  freeink::ui::RadioGroupProps rgp;
  rgp.options = opts.data();
  rgp.count = static_cast<uint8_t>(opts.size());
  rgp.selectedValue = static_cast<int16_t>(selectedValue);
  rgp.action = act;
  rgp.gap = static_cast<int16_t>(gap);
  rgp.radius = static_cast<uint8_t>(tokens.controlRadius);
  rgp.text.font = freeink::ui::GfxRendererTarget::FONT_BODY;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::radioGroup(*frame, rect, rgp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawTable(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();

  int x = 0, y = 0, w = 400, h = 200;
  int rows = 0;
  int cols = 0;
  int rowHeight = 28;
  int padding = 6;
  bool headerRow = true;
  std::vector<std::string> cellStrings;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "rows");
    if (lua_isnumber(L, -1)) rows = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "cols");
    if (lua_isnumber(L, -1)) cols = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "rowHeight");
    if (lua_isnumber(L, -1)) rowHeight = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "padding");
    if (lua_isnumber(L, -1)) padding = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "headerRow");
    if (lua_isboolean(L, -1)) headerRow = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "cells");
    if (lua_istable(L, -1)) {
      const int count = static_cast<int>(lua_rawlen(L, -1));
      for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isstring(L, -1)) {
          cellStrings.push_back(lua_tostring(L, -1));
        } else if (lua_istable(L, -1)) {
          const int subCount = static_cast<int>(lua_rawlen(L, -1));
          if (cols == 0) cols = subCount;
          for (int j = 1; j <= subCount; ++j) {
            lua_rawgeti(L, -1, j);
            cellStrings.push_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
            lua_pop(L, 1);
          }
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  if (cols <= 0) cols = 1;
  if (rows <= 0) rows = static_cast<int>(cellStrings.size() / cols);
  while (cellStrings.size() < static_cast<size_t>(rows * cols)) {
    cellStrings.push_back("");
  }

  std::vector<const char*> cellPtrs;
  cellPtrs.reserve(cellStrings.size());
  for (const auto& s : cellStrings) {
    cellPtrs.push_back(s.c_str());
  }

  freeink::ui::TableProps tp;
  tp.cells = cellPtrs.data();
  tp.rows = static_cast<uint8_t>(rows);
  tp.cols = static_cast<uint8_t>(cols);
  tp.rowHeight = static_cast<int16_t>(rowHeight);
  tp.padding = static_cast<int16_t>(padding);
  tp.headerRow = headerRow;
  tp.text.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  tp.grid = freeink::ui::Paint::solid(freeink::ui::Color::Black);
  tp.gridWidth = 1;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(rowHeight * rows)};
  freeink::ui::table(*frame, rect, tp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, rowHeight * rows);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawMetricCard(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();
  const auto& tokens = ctx->uiHost->getTokens();

  int x = 0, y = 0, w = 180, h = 100;
  std::string label;
  std::string value;
  std::string unit;
  std::string caption;
  bool centered = true;
  int radius = tokens.listRowRadius > 0 ? tokens.listRowRadius : tokens.controlRadius;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    if (lua_isnumber(L, -1)) h = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "label");
    if (lua_isstring(L, -1)) label = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "value");
    if (lua_isstring(L, -1)) value = lua_tostring(L, -1);
    else if (lua_isnumber(L, -1)) value = std::to_string(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "unit");
    if (lua_isstring(L, -1)) unit = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "caption");
    if (lua_isstring(L, -1)) caption = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "centered");
    if (lua_isboolean(L, -1)) centered = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "radius");
    if (lua_isnumber(L, -1)) radius = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
  }

  freeink::ui::ActionId act = freeink::ui::NO_ACTION;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "onClick");
    if (lua_isfunction(L, -1)) {
      act = ctx->uiHost->registerCallback(L, lua_gettop(L), 0, "metricCard");
    }
    lua_pop(L, 1);
  }

  freeink::ui::MetricCardProps mcp;
  mcp.label = label.empty() ? nullptr : label.c_str();
  mcp.value = value.empty() ? nullptr : value.c_str();
  mcp.unit = unit.empty() ? nullptr : unit.c_str();
  mcp.caption = caption.empty() ? nullptr : caption.c_str();
  mcp.centered = centered;
  mcp.action = act;
  mcp.labelText.font = freeink::ui::GfxRendererTarget::FONT_SMALL;
  mcp.valueText.font = freeink::ui::GfxRendererTarget::FONT_TITLE;
  mcp.captionText.font = freeink::ui::GfxRendererTarget::FONT_SMALL;

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::metricCard(*frame, rect, mcp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
  return 1;
}

static int l_ui_drawContextMenu(lua_State* L) {
  auto* ctx = getContext(L);
  if (!ctx || !ctx->uiHost || !ctx->uiHost->currentFrame()) {
    lua_newtable(L);
    return 1;
  }
  auto* frame = ctx->uiHost->currentFrame();

  int x = 40, y = 100, w = 240, h = 0;
  std::string title;
  bool dimBackground = true;
  std::vector<std::string> itemLabels;
  std::vector<freeink::ui::DialogOption> options;

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "x");
    if (lua_isnumber(L, -1)) x = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    if (lua_isnumber(L, -1)) y = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    if (lua_isnumber(L, -1)) w = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "title");
    if (lua_isstring(L, -1)) title = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "dimBackground");
    if (lua_isboolean(L, -1)) dimBackground = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "items");
    if (lua_istable(L, -1)) {
      const int count = static_cast<int>(lua_rawlen(L, -1));
      for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_istable(L, -1)) {
          lua_getfield(L, -1, "label");
          std::string l = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
          lua_pop(L, 1);
          itemLabels.push_back(l);

          freeink::ui::ActionId act = freeink::ui::NO_ACTION;
          lua_getfield(L, -1, "onClick");
          if (lua_isfunction(L, -1)) {
            act = ctx->uiHost->registerCallback(L, lua_gettop(L), static_cast<int16_t>(i), "contextItem");
          }
          lua_pop(L, 1);

          freeink::ui::DialogOption opt;
          opt.label = itemLabels.back().c_str();
          opt.action = act;
          opt.value = static_cast<int16_t>(i);
          options.push_back(opt);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  const int rowH = 40;
  const int titleH = title.empty() ? 0 : 30;
  h = 20 + titleH + static_cast<int>(options.size()) * (rowH + 2);

  freeink::ui::ContextMenuProps cmp;
  cmp.title = title.empty() ? nullptr : title.c_str();
  cmp.options = options.data();
  cmp.optionCount = static_cast<uint8_t>(options.size());
  cmp.dimBackground = dimBackground;
  cmp.titleText.font = freeink::ui::GfxRendererTarget::FONT_TITLE;
  cmp.itemText.font = freeink::ui::GfxRendererTarget::FONT_BODY;
  cmp.rowHeight = static_cast<int16_t>(rowH);

  freeink::ui::Rect rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  freeink::ui::contextMenu(*frame, rect, cmp);

  lua_newtable(L);
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, w);
  lua_setfield(L, -2, "w");
  lua_pushinteger(L, h);
  lua_setfield(L, -2, "h");
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

  // Register ui functions (Phase 1 & 2)
  static const luaL_Reg uiFuncs[] = {
      {"getTheme", l_ui_getTheme},
      {"setTheme", l_ui_setTheme},
      {"drawHeader", l_ui_drawHeader},
      {"drawTabBar", l_ui_drawTabBar},
      {"drawButton", l_ui_drawButton},
      {"drawCard", l_ui_drawCard},
      {"drawBadge", l_ui_drawBadge},
      {"drawToggle", l_ui_drawToggle},
      {"drawDialog", l_ui_drawDialog},
      {"drawToast", l_ui_drawToast},
      {"drawSlider", l_ui_drawSlider},
      {"drawCapsuleSlider", l_ui_drawCapsuleSlider},
      {"drawProgressBar", l_ui_drawProgressBar},
      {"drawCheckbox", l_ui_drawCheckbox},
      {"drawSettingRow", l_ui_drawSettingRow},
      {"drawToggleRow", l_ui_drawToggleRow},
      {"drawStepperRow", l_ui_drawStepperRow},
      {"drawRadioGroup", l_ui_drawRadioGroup},
      {"drawTable", l_ui_drawTable},
      {"drawMetricCard", l_ui_drawMetricCard},
      {"drawContextMenu", l_ui_drawContextMenu},
      {nullptr, nullptr},
  };
  registerModule(L, "ui", uiFuncs, ctx);

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
