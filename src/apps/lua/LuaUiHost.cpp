#include "LuaUiHost.h"

#include <BoardConfig.h>
#include <CrossPointSettings.h>
#include <Logging.h>

#include "components/UIScale.h"
#include "components/UIThemeTokens.h"
#include <FreeInkUIIcon.h>
#include "components/icons/headerIcons.h"
#include "fontIds.h"

namespace lua_host {

const freeink::ui::BitmapRef LuaUiHost::getHeaderBackIcon() {
  return freeink::ui::bitmapFromIcon(icon_header_back_32);
}

LuaUiHost::LuaUiHost(const GfxRenderer& renderer)
    : renderer_(renderer), target_(renderer, BoardConfig::hasTouch()) {
  const auto spec = uiScaleSpec();
  target_.setFont(freeink::ui::GfxRendererTarget::FONT_SMALL, spec.smallFontId);
  target_.setFont(freeink::ui::GfxRendererTarget::FONT_BODY, spec.bodyFontId);
  target_.setFont(freeink::ui::GfxRendererTarget::FONT_TITLE, spec.titleFontId);
  target_.setFont(freeink::ui::GfxRendererTarget::FONT_LABEL, SMALL_FONT_ID);

  const auto& settings = CrossPointSettings::getInstance();
  if (settings.uiTheme == CrossPointSettings::ROUNDEDRAFF) {
    currentTheme_ = UiThemeType::RoundedRaff;
  } else if (settings.uiTheme == CrossPointSettings::CLASSIC) {
    currentTheme_ = UiThemeType::Classic;
  } else {
    currentTheme_ = UiThemeType::Lyra;
  }

  refreshTokens();
}

LuaUiHost::~LuaUiHost() {
  detachLua();
}

void LuaUiHost::detachLua() {
  clearCallbacks(activeL_);
  activeL_ = nullptr;
}

void LuaUiHost::setTheme(UiThemeType type) {
  currentTheme_ = type;
  refreshTokens();
}

void LuaUiHost::setThemeByName(const std::string& name) {
  if (name == "RoundedRaff" || name == "roundedraff" || name == "rounded") {
    setTheme(UiThemeType::RoundedRaff);
  } else if (name == "Classic" || name == "classic") {
    setTheme(UiThemeType::Classic);
  } else {
    setTheme(UiThemeType::Lyra);
  }
}

std::string LuaUiHost::getThemeName() const {
  switch (currentTheme_) {
    case UiThemeType::RoundedRaff:
      return "RoundedRaff";
    case UiThemeType::Classic:
      return "Classic";
    case UiThemeType::Lyra:
    default:
      return "Lyra";
  }
}

void LuaUiHost::refreshTokens() {
  tokens_ = freeink::ui::themeTokensForLineHeight(target_.lineHeight(freeink::ui::GfxRendererTarget::FONT_BODY));

  switch (currentTheme_) {
    case UiThemeType::RoundedRaff:
      tokens_.headerHeight = 48;
      tokens_.headerUnderline = 0;
      tokens_.headerTitleAlign = freeink::ui::TextAlign::Center;
      tokens_.controlRadius = 12;
      tokens_.listRowRadius = 10;
      tokens_.sheetRadius = 12;
      tokens_.capsuleRadius = 16;
      break;

    case UiThemeType::Classic:
      tokens_.headerHeight = 44;
      tokens_.headerUnderline = 1;
      tokens_.headerTitleAlign = freeink::ui::TextAlign::Left;
      tokens_.controlRadius = 0;
      tokens_.listRowRadius = 0;
      tokens_.sheetRadius = 0;
      tokens_.capsuleRadius = 0;
      break;

    case UiThemeType::Lyra:
    default:
      tokens_.headerHeight = 44;
      tokens_.headerUnderline = 1;
      tokens_.headerTitleAlign = freeink::ui::TextAlign::Left;
      tokens_.controlRadius = 6;
      tokens_.listRowRadius = 6;
      tokens_.sheetRadius = 8;
      tokens_.capsuleRadius = 12;
      break;
  }
}

void LuaUiHost::clearCallbacks(lua_State* L) {
  if (L) {
    for (int ref : luaRefsToClean_) {
      if (ref != -1 && ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
      }
    }
  }
  luaRefsToClean_.clear();
  callbacks_.clear();
}

void LuaUiHost::beginFrame(lua_State* L) {
  clearCallbacks(activeL_);
  activeL_ = L;
  nextActionId_ = 1;

  interactions_.beginPublishCycle();
  interactions_.clear();

  deviceContext_ = target_.deviceContext();
  emptySnap_ = freeink::ui::InputSnapshot{};
  frame_ = std::make_unique<freeink::ui::Frame<INTERACTION_CAPACITY>>(
      target_, deviceContext_, emptySnap_, interactions_);
}

void LuaUiHost::endFrame() {
  interactions_.publish();
}

freeink::ui::ActionId LuaUiHost::registerCallback(lua_State* L, int funcIndex, int16_t value,
                                                  const std::string& name) {
  return registerCallbackWithInvoker(L, funcIndex, nullptr, value, name);
}

freeink::ui::ActionId LuaUiHost::registerCallbackWithInvoker(lua_State* L, int funcIndex,
                                                            UiInvoker invoker, int16_t value,
                                                            const std::string& name) {
  freeink::ui::ActionId action = nextActionId_++;
  int ref = -1;
  if (L && funcIndex != 0 && lua_isfunction(L, funcIndex)) {
    lua_pushvalue(L, funcIndex);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    luaRefsToClean_.push_back(ref);
  }
  callbacks_[action] = UiCallback{ref, name, value, std::move(invoker)};
  return action;
}

bool LuaUiHost::dispatchTouch(int x, int y, lua_State* L) {
  const size_t count = interactions_.publishedCount();
  const freeink::ui::Interaction* data = interactions_.publishedData();

  for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
    const freeink::ui::Interaction& hit = data[i];
    if (hit.rect.contains(static_cast<int16_t>(x), static_cast<int16_t>(y)) &&
        !freeink::ui::hasState(hit.state, freeink::ui::StateDisabled)) {
      auto it = callbacks_.find(hit.action);
      if (it != callbacks_.end()) {
        const UiCallback& cb = it->second;

        // 1. If explicit Lua callback is registered:
        if (cb.luaFuncRef != -1 && L) {
          const int errIdx = lua_gettop(L) + 1;
          lua_pushcfunction(L, [](lua_State* s) -> int {
            const char* msg = lua_tostring(s, 1);
            LOG_ERR("LUA_UI", "UI Callback Error: %s", msg ? msg : "");
            return 1;
          });
          lua_rawgeti(L, LUA_REGISTRYINDEX, cb.luaFuncRef);
          if (lua_isfunction(L, -1)) {
            int numArgs = 1;
            if (cb.invoker) {
              numArgs = cb.invoker(L, x, y, hit.value != 0 ? hit.value : cb.value);
            } else {
              lua_pushinteger(L, hit.value != 0 ? hit.value : cb.value);
            }
            lua_pcall(L, numArgs, 0, errIdx);
          } else {
            lua_pop(L, 1);
          }
          lua_remove(L, errIdx);
          return true;
        }

        // 2. If it's a default back button action
        if (cb.name == "back" && L) {
          lua_getglobal(L, "onBack");
          if (lua_isfunction(L, -1)) {
            lua_pcall(L, 0, 0, 0);
          } else {
            lua_pop(L, 1);
          }
          return true;
        }

        return true;  // Handled hit even if no callback
      }
    }
  }

  return false;  // Fall through to onTouch(x, y)
}

}  // namespace lua_host
