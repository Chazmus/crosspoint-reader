#pragma once

#include <mutex>
#include <string>

#include "LuaBindings.h"
#include "LuaUiHost.h"
#include "activities/Activity.h"
#include "activities/util/KeyboardEntryActivity.h"

struct AppDescriptor;

class LuaAppActivity final : public Activity {
 private:
  std::string scriptPath_;
  std::string appDir_;
  std::string appTitle_;
  std::string orientationStr_ = "portrait";
  GfxRenderer::Orientation origOrientation_ = GfxRenderer::Orientation::Portrait;
  lua_State* L_ = nullptr;
  mutable std::recursive_mutex luaMutex_;
  std::unique_ptr<lua_host::LuaUiHost> uiHost_;
  lua_host::HostContext hostCtx_;
  bool hasError_ = false;
  std::string errorMessage_;
  unsigned long lastUpdateMs_ = 0;
  bool wifiStartedByUs_ = false;
  bool wifiAutoDisconnect_ = false;
  int wifiCallbackRef_ = LUA_NOREF;
  int promptConfirmRef_ = LUA_NOREF;
  int promptCancelRef_ = LUA_NOREF;

  void handleLuaError(const char* context);
  bool callLuaFunction(const char* funcName);
  bool callLuaFunction(const char* funcName, int arg1, int arg2);
  bool callLuaFunction(const char* funcName, float arg1);
  void onWifiSelectionComplete(bool success);
  void onPromptTextComplete(const ActivityResult& res);

 public:
  LuaAppActivity(std::string scriptPath, std::string appDir, std::string appTitle, std::string orientationStr,
                 GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~LuaAppActivity() override;

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  void onExit() override;

  void connectWifi(int callbackRef, bool autoDisconnect = false);
  void disconnectWifi();

  void promptText(const std::string& title, const std::string& initialText, size_t maxLength,
                  InputType inputType, int confirmRef, int cancelRef);

  static std::unique_ptr<Activity> create(const AppDescriptor& desc, GfxRenderer& renderer, MappedInputManager& input);
  static bool renderSleepScreen(const AppDescriptor& desc, GfxRenderer& renderer);
};
