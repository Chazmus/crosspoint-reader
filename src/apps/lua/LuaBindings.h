#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <string>

class GfxRenderer;
class MappedInputManager;
class Activity;
class LuaAppActivity;

namespace lua_host {

struct HostContext {
  GfxRenderer* renderer = nullptr;
  MappedInputManager* input = nullptr;
  Activity* activity = nullptr;
  LuaAppActivity* luaApp = nullptr;
  std::string appDir;

  std::string resolvePath(const char* path) const {
    if (!path || !*path) return "";
    if (path[0] == '/') return std::string(path);
    if (!appDir.empty() && appDir.back() != '/') {
      return appDir + "/" + path;
    }
    return appDir + path;
  }
};

void registerBindings(lua_State* L, HostContext* ctx);

}  // namespace lua_host
