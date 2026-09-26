#include "LuaAppScanner.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include "LuaAppActivity.h"
#include "components/UITheme.h"

namespace lua_host {
namespace {

UIIcon parseIcon(const char* iconStr) {
  if (!iconStr) return UIIcon::Blocks;
  if (strcmp(iconStr, "Book") == 0) return UIIcon::Book;
  if (strcmp(iconStr, "Folder") == 0) return UIIcon::Folder;
  if (strcmp(iconStr, "Text") == 0) return UIIcon::Text;
  if (strcmp(iconStr, "Image") == 0) return UIIcon::Image;
  if (strcmp(iconStr, "File") == 0) return UIIcon::File;
  if (strcmp(iconStr, "Recent") == 0 || strcmp(iconStr, "Clock") == 0) return UIIcon::Recent;
  if (strcmp(iconStr, "Settings") == 0) return UIIcon::Settings;
  if (strcmp(iconStr, "Transfer") == 0) return UIIcon::Transfer;
  if (strcmp(iconStr, "Library") == 0) return UIIcon::Library;
  if (strcmp(iconStr, "Wifi") == 0) return UIIcon::Wifi;
  if (strcmp(iconStr, "Hotspot") == 0) return UIIcon::Hotspot;
  if (strcmp(iconStr, "Bookmark") == 0) return UIIcon::Bookmark;
  if (strcmp(iconStr, "Usb") == 0) return UIIcon::Usb;
  return UIIcon::Blocks;
}

void scanDirectory(const char* basePath, std::vector<AppDescriptor>& outApps) {
  HalFile root = Storage.open(basePath);
  if (!root || !root.isDirectory()) return;

  char entryName[64];
  for (auto entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    if (!entry.getName(entryName, sizeof(entryName))) continue;
    if (entryName[0] == '.') continue;  // Skip hidden files/directories

    if (entry.isDirectory()) {
      std::string appDir = std::string(basePath) + "/" + entryName;
      std::string mainLua = appDir + "/main.lua";

      if (Storage.exists(mainLua.c_str())) {
        AppDescriptor desc;
        desc.idStr = entryName;
        desc.titleStr = entryName;
        desc.descStr = "Lua Application";
        desc.appDir = appDir;
        desc.scriptPath = mainLua;
        desc.icon = UIIcon::Blocks;
        desc.isLuaApp = true;
        desc.createInstance = &LuaAppActivity::create;

        // Check for optional manifest.json
        std::string manifestPath = appDir + "/manifest.json";
        HalFile mFile;
        if (Storage.openFileForRead("LUA", manifestPath, mFile)) {
          JsonDocument doc;
          if (deserializeJson(doc, mFile) == DeserializationError::Ok) {
            if (doc["id"].is<const char*>()) desc.idStr = doc["id"].as<const char*>();
            if (doc["title"].is<const char*>()) desc.titleStr = doc["title"].as<const char*>();
            if (doc["description"].is<const char*>()) desc.descStr = doc["description"].as<const char*>();
            if (doc["icon"].is<const char*>()) desc.icon = parseIcon(doc["icon"].as<const char*>());
            if (doc["orientation"].is<const char*>()) {
              desc.orientationStr = doc["orientation"].as<const char*>();
            }
            if (doc["sleepScreen"].as<bool>()) {
              desc.renderSleepScreen = &LuaAppActivity::renderSleepScreen;
            }
          }
        }

        // Set pointers to internal std::strings
        desc.id = desc.idStr.c_str();
        desc.title = desc.titleStr.c_str();
        desc.description = desc.descStr.c_str();

        LOG_INF("LUA", "Discovered SD app: %s (%s)", desc.resolveId(), desc.resolveTitle());
        outApps.push_back(std::move(desc));
      }
    } else {
      // Check for standalone .lua files, e.g. /apps/counter.lua
      const size_t len = strlen(entryName);
      if (len > 4 && strcmp(entryName + len - 4, ".lua") == 0) {
        std::string baseId(entryName, len - 4);
        AppDescriptor desc;
        desc.idStr = baseId;
        desc.titleStr = baseId;
        desc.descStr = "Lua Script";
        desc.appDir = basePath;
        desc.scriptPath = std::string(basePath) + "/" + entryName;
        desc.icon = UIIcon::Blocks;
        desc.isLuaApp = true;
        desc.createInstance = &LuaAppActivity::create;

        desc.id = desc.idStr.c_str();
        desc.title = desc.titleStr.c_str();
        desc.description = desc.descStr.c_str();

        LOG_INF("LUA", "Discovered standalone SD script: %s", desc.scriptPath.c_str());
        outApps.push_back(std::move(desc));
      }
    }
  }
}

}  // namespace

std::vector<AppDescriptor> scanSdApps() {
  std::vector<AppDescriptor> apps;
  apps.reserve(16);
  scanDirectory("/apps", apps);
  scanDirectory("/.crosspoint/apps", apps);

  // Re-link const char* pointers to avoid invalidation during vector moves
  for (auto& app : apps) {
    if (!app.idStr.empty()) app.id = app.idStr.c_str();
    if (!app.titleStr.empty()) app.title = app.titleStr.c_str();
    if (!app.descStr.empty()) app.description = app.descStr.c_str();
  }

  return apps;
}

}  // namespace lua_host
