#pragma once

#include <functional>
#include <string>

#include "AppCatalog.h"

class AppInstaller {
 public:
  using ProgressCallback =
      std::function<void(size_t fileIndex, size_t fileCount, size_t fileDownloaded, size_t fileTotal)>;

  static bool installApp(const CatalogApp& app, ProgressCallback onProgress, bool* cancelFlag, std::string& outError);

  static bool uninstallApp(const std::string& appId, std::string& outError);
};
