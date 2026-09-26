#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "AppDescriptor.h"

class AppRegistry {
 public:
  static const std::vector<AppDescriptor>& getApps();
  static void refreshApps();
  static size_t getAppCount();
  static const AppDescriptor* getAppAt(size_t index);
  static const AppDescriptor* getAppById(std::string_view id);
  static const AppDescriptor* getSleepScreenApp(std::string_view preferredId = {});
  static bool hasSleepScreenProvider();
};
