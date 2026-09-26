#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "components/UITheme.h"

class Activity;
class GfxRenderer;
class MappedInputManager;

struct AppDescriptor {
  std::string idStr;
  std::string titleStr;
  std::string descStr;
  std::string scriptPath;
  std::string appDir;
  std::string orientationStr = "portrait";
  bool isLuaApp = false;

  const char* id = nullptr;
  const char* title = nullptr;
  const char* description = nullptr;
  const char* (*getTitle)() = nullptr;
  const char* (*getDescription)() = nullptr;
  UIIcon icon = UIIcon::Blocks;
  std::unique_ptr<Activity> (*createInstance)(const AppDescriptor& desc, GfxRenderer& renderer,
                                              MappedInputManager& input) = nullptr;
  bool (*renderSleepScreen)(const AppDescriptor& desc, GfxRenderer& renderer) = nullptr;

  const char* resolveId() const {
    if (!idStr.empty()) return idStr.c_str();
    return id ? id : "";
  }

  const char* resolveTitle() const {
    if (getTitle) {
      const char* t = getTitle();
      if (t && *t) return t;
    }
    if (!titleStr.empty()) return titleStr.c_str();
    return title ? title : resolveId();
  }

  const char* resolveDescription() const {
    if (getDescription) {
      const char* d = getDescription();
      if (d) return d;
    }
    if (!descStr.empty()) return descStr.c_str();
    return description ? description : "";
  }
};
