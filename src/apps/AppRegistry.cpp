#include "AppRegistry.h"

#include <vector>

#include "lua/LuaAppScanner.h"

namespace {
std::vector<AppDescriptor> s_apps;
bool s_initialized = false;

void ensureInitialized() {
  if (s_initialized) return;
  s_apps.clear();

  // Discover dynamic Lua apps from SD card (/apps and /.crosspoint/apps)
  auto sdApps = lua_host::scanSdApps();
  s_apps.reserve(sdApps.size());
  for (auto& app : sdApps) {
    s_apps.push_back(std::move(app));
  }

  // Re-link string pointers for all apps
  for (auto& app : s_apps) {
    if (!app.idStr.empty()) app.id = app.idStr.c_str();
    if (!app.titleStr.empty()) app.title = app.titleStr.c_str();
    if (!app.descStr.empty()) app.description = app.descStr.c_str();
  }

  s_initialized = true;
}
}  // namespace

void AppRegistry::refreshApps() {
  s_initialized = false;
  ensureInitialized();
}

const std::vector<AppDescriptor>& AppRegistry::getApps() {
  ensureInitialized();
  return s_apps;
}

size_t AppRegistry::getAppCount() { return getApps().size(); }

const AppDescriptor* AppRegistry::getAppAt(const size_t index) {
  const auto& apps = getApps();
  if (index >= apps.size()) return nullptr;
  return &apps[index];
}

const AppDescriptor* AppRegistry::getAppById(const std::string_view id) {
  for (const auto& app : getApps()) {
    if (id == app.resolveId()) return &app;
  }
  return nullptr;
}

const AppDescriptor* AppRegistry::getSleepScreenApp(const std::string_view preferredId) {
  if (!preferredId.empty()) {
    const auto* app = getAppById(preferredId);
    if (app && app->renderSleepScreen) return app;
  }
  for (const auto& app : getApps()) {
    if (app.renderSleepScreen) return &app;
  }
  return nullptr;
}

bool AppRegistry::hasSleepScreenProvider() {
  for (const auto& app : getApps()) {
    if (app.renderSleepScreen) return true;
  }
  return false;
}
