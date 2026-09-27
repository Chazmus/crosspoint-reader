#pragma once

#include <FreeInkUICore.h>
#include <FreeInkUIGfxRenderer.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class GfxRenderer;

namespace lua_host {

enum class UiThemeType { Lyra = 0, RoundedRaff = 1, Classic = 2 };

using UiInvoker = std::function<int(lua_State* L, int touchX, int touchY, int16_t value)>;

struct UiCallback {
  int luaFuncRef = -1;
  std::string name;
  int16_t value = 0;
  UiInvoker invoker = nullptr;
};

class LuaUiHost {
 public:
  static constexpr size_t INTERACTION_CAPACITY = 64;

  explicit LuaUiHost(const GfxRenderer& renderer);
  ~LuaUiHost();

  void setTheme(UiThemeType type);
  void setThemeByName(const std::string& name);
  UiThemeType getThemeType() const { return currentTheme_; }
  std::string getThemeName() const;
  bool isTabPillFullSlot() const { return currentTheme_ == UiThemeType::RoundedRaff; }
  const freeink::ui::ThemeTokens& getTokens() const { return tokens_; }

  void beginFrame(lua_State* L);
  void endFrame();
  void detachLua();

  bool dispatchTouch(int x, int y, lua_State* L);

  freeink::ui::ActionId registerCallback(lua_State* L, int funcIndex, int16_t value = 0,
                                         const std::string& name = "");
  freeink::ui::ActionId registerCallbackWithInvoker(lua_State* L, int funcIndex, UiInvoker invoker,
                                                    int16_t value = 0, const std::string& name = "");

  freeink::ui::Frame<INTERACTION_CAPACITY>* currentFrame() { return frame_.get(); }
  freeink::ui::GfxRendererTarget& drawTarget() { return target_; }

  static const freeink::ui::BitmapRef getHeaderBackIcon();

 private:
  const GfxRenderer& renderer_;
  freeink::ui::GfxRendererTarget target_;
  freeink::ui::DeviceContext deviceContext_;
  UiThemeType currentTheme_ = UiThemeType::Lyra;
  freeink::ui::ThemeTokens tokens_;

  freeink::ui::InteractionBuffer<INTERACTION_CAPACITY> interactions_;
  freeink::ui::InputSnapshot emptySnap_{};
  std::unique_ptr<freeink::ui::Frame<INTERACTION_CAPACITY>> frame_;

  lua_State* activeL_ = nullptr;
  freeink::ui::ActionId nextActionId_ = 1;
  std::map<freeink::ui::ActionId, UiCallback> callbacks_;
  std::vector<int> luaRefsToClean_;

  void refreshTokens();
  void clearCallbacks(lua_State* L);
};

}  // namespace lua_host
