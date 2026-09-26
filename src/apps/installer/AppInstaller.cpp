#include "AppInstaller.h"

#include <HalStorage.h>
#include <Logging.h>

#include "apps/AppRegistry.h"

#if defined(ESP32) || defined(ARDUINO)
#include <Esp.h>
#include "network/HttpDownloader.h"
#endif

namespace {
void ensureParentDir(const std::string& filePath) {
  size_t lastSlash = filePath.find_last_of('/');
  if (lastSlash != std::string::npos && lastSlash > 0) {
    std::string dir = filePath.substr(0, lastSlash);
    Storage.ensureDirectoryExists(dir.c_str());
  }
}
}  // namespace

bool AppInstaller::installApp(const CatalogApp& app, ProgressCallback onProgress, bool* cancelFlag,
                              std::string& outError) {
  if (app.id.empty()) {
    outError = "Invalid app ID";
    return false;
  }

#if !defined(ESP32) && !defined(ARDUINO)
  outError = "Host mock: download not supported";
  return false;
#else
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP) {
    outError = "Low memory for download";
    return false;
  }

  std::string appDir = "/apps/" + app.id;
  Storage.ensureDirectoryExists("/apps");
  Storage.ensureDirectoryExists(appDir.c_str());

  size_t totalFiles = app.files.size();
  for (size_t i = 0; i < totalFiles; ++i) {
    if (cancelFlag && *cancelFlag) {
      outError = "Installation cancelled";
      return false;
    }

    const auto& file = app.files[i];
    std::string destPath = appDir + "/" + file;
    ensureParentDir(destPath);

    std::string filePathInRepo = app.path;
    if (!filePathInRepo.empty() && filePathInRepo.back() != '/') {
      filePathInRepo += "/";
    }
    filePathInRepo += file;

    std::string url = AppCatalog::buildRawUrl(app.sourceRepo, app.sourceBranch, filePathInRepo);
    LOG_INF("APPINST", "Downloading [%zu/%zu] %s -> %s", i + 1, totalFiles, url.c_str(), destPath.c_str());

    auto progressHandler = [&](size_t downloaded, size_t total) {
      if (onProgress) {
        onProgress(i, totalFiles, downloaded, total);
      }
    };

    auto res = HttpDownloader::downloadToFile(url, destPath, progressHandler, cancelFlag);
    if (res != HttpDownloader::OK) {
      LOG_ERR("APPINST", "Failed downloading %s: code %d", file.c_str(), static_cast<int>(res));
      outError = "Download failed: " + file;
      return false;
    }
  }

  // Refresh AppRegistry so new app appears immediately
  AppRegistry::refreshApps();
  LOG_INF("APPINST", "Successfully installed %s (%s)", app.name.c_str(), app.id.c_str());
  return true;
#endif
}

bool AppInstaller::uninstallApp(const std::string& appId, std::string& outError) {
  if (appId.empty()) {
    outError = "Invalid app ID";
    return false;
  }

  std::string appDir = "/apps/" + appId;
  if (!Storage.exists(appDir.c_str())) {
    AppRegistry::refreshApps();
    return true;
  }

  if (!Storage.removeDir(appDir.c_str())) {
    LOG_ERR("APPINST", "Failed to remove directory: %s", appDir.c_str());
    outError = "Failed to remove " + appDir;
    return false;
  }

  AppRegistry::refreshApps();
  LOG_INF("APPINST", "Uninstalled app: %s", appId.c_str());
  return true;
}
