#include "AppCatalog.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cctype>
#include <cstring>

#if defined(ESP32) || defined(ARDUINO)
#include <Arduino.h>
#include <Esp.h>

#include "network/HttpDownloader.h"
#else
#include <chrono>
#endif

namespace {
static constexpr const char* TMP_CATALOG_FILE = "/.crosspoint/tmp_cat.json";
static constexpr const char* TMP_VALIDATE_FILE = "/.crosspoint/tmp_cat_val.json";

std::string trim(const std::string& str) {
  size_t start = 0;
  while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
    start++;
  }
  size_t end = str.size();
  while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
    end--;
  }
  return str.substr(start, end - start);
}
}  // namespace

std::string AppCatalog::normalizeRepo(const std::string& input) {
  std::string s = trim(input);
  if (s.empty()) return "";

  // Check protocol prefix: if a protocol is given, it MUST be followed by github.com/
  static constexpr const char* HTTP_PREFIX = "http://";
  static constexpr const char* HTTPS_PREFIX = "https://";
  if (s.rfind(HTTPS_PREFIX, 0) == 0) {
    s = s.substr(std::strlen(HTTPS_PREFIX));
    if (s.rfind("github.com/", 0) != 0) return "";
    s = s.substr(std::strlen("github.com/"));
  } else if (s.rfind(HTTP_PREFIX, 0) == 0) {
    s = s.substr(std::strlen(HTTP_PREFIX));
    if (s.rfind("github.com/", 0) != 0) return "";
    s = s.substr(std::strlen("github.com/"));
  } else if (s.rfind("github.com/", 0) == 0) {
    s = s.substr(std::strlen("github.com/"));
  }

  // Strip leading slashes
  while (!s.empty() && s.front() == '/') {
    s.erase(s.begin());
  }

  // Strip trailing .git
  static constexpr const char* GIT_SUFFIX = ".git";
  if (s.size() > 4 && s.rfind(GIT_SUFFIX) == s.size() - 4) {
    s = s.substr(0, s.size() - 4);
  }

  // Strip trailing slashes
  while (!s.empty() && s.back() == '/') {
    s.pop_back();
  }

  // Must have at least one '/' dividing owner and repo
  size_t slashPos = s.find('/');
  if (slashPos == std::string::npos || slashPos == 0 || slashPos == s.size() - 1) {
    return "";
  }
  if (s.find('/', slashPos + 1) != std::string::npos) {
    // Truncate any extra path segments to owner/repo
    s = s.substr(0, s.find('/', slashPos + 1));
    slashPos = s.find('/');
  }

  std::string owner = s.substr(0, slashPos);
  std::string repo = s.substr(slashPos + 1);
  if (owner.empty() || repo.empty()) return "";

  // GitHub usernames: alphanumeric and hyphens only
  for (char c : owner) {
    if (c != '-' && !std::isalnum(static_cast<unsigned char>(c))) return "";
  }
  // GitHub repo names: alphanumeric, hyphens, underscores, dots
  for (char c : repo) {
    if (c != '-' && c != '_' && c != '.' && !std::isalnum(static_cast<unsigned char>(c))) return "";
  }

  return owner + "/" + repo;
}

std::string AppCatalog::buildRawUrl(const std::string& repo, const std::string& branch, const std::string& path,
                                    const bool cacheBust) {
  std::string b = branch.empty() ? "main" : branch;
  std::string url = "https://raw.githubusercontent.com/" + repo + "/" + b;
  if (!path.empty()) {
    if (path.front() != '/') url += "/";
    url += path;
  }
  if (cacheBust) {
#if defined(ESP32) || defined(ARDUINO)
    const uint32_t t = millis();
#else
    const auto t =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
#endif
    url += (url.find('?') == std::string::npos) ? "?t=" : "&t=";
    url += std::to_string(t);
  }
  return url;
}

bool CatalogApp::isInstalled() const {
  if (id.empty()) return false;
  std::string manifestPath = "/apps/" + id + "/manifest.json";
  std::string mainPath = "/apps/" + id + "/main.lua";
  return Storage.exists(manifestPath.c_str()) || Storage.exists(mainPath.c_str());
}

std::string CatalogApp::getInstalledVersion() const {
  if (id.empty()) return "";
  std::string manifestPath = "/apps/" + id + "/manifest.json";
  HalFile file;
  if (!Storage.openFileForRead("CATALOG", manifestPath.c_str(), file)) {
    return isInstalled() ? "1.0.0" : "";
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  if (err) return "1.0.0";
  const char* v = doc["version"] | "1.0.0";
  return v ? std::string(v) : "1.0.0";
}

bool CatalogApp::hasUpdate() const {
  if (!isInstalled()) return false;
  std::string installedVer = getInstalledVersion();
  return !version.empty() && !installedVer.empty() && installedVer != version;
}

bool AppCatalog::parseCatalogJson(const char* jsonContent, const std::string& repo, const std::string& branch,
                                  std::string& outCatalogName, std::vector<CatalogApp>& outApps,
                                  std::string& outError) {
  if (!jsonContent || *jsonContent == '\0') {
    outError = "Empty catalog response";
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonContent);
  if (err) {
    outError = std::string("JSON parse error: ") + err.c_str();
    return false;
  }

  const char* name = doc["name"] | "";
  outCatalogName = (name && *name) ? name : repo;

  JsonArrayConst appsArr = doc["apps"].as<JsonArrayConst>();
  if (appsArr.isNull()) {
    outError = "Catalog missing 'apps' array";
    return false;
  }

  for (JsonObjectConst obj : appsArr) {
    const char* appId = obj["id"] | "";
    if (!appId || !*appId) continue;

    CatalogApp app;
    app.id = appId;
    app.name = obj["name"] | obj["title"] | appId;
    app.version = obj["version"] | "1.0.0";
    app.author = obj["author"] | "";
    app.description = obj["description"] | "";
    app.category = obj["category"] | "";
    app.icon = obj["icon"] | "Blocks";

    const char* p = obj["path"] | "";
    app.path = (p && *p) ? p : ("apps/" + app.id);

    app.sourceRepo = repo;
    app.sourceBranch = branch.empty() ? "main" : branch;

    JsonArrayConst filesArr = obj["files"].as<JsonArrayConst>();
    if (!filesArr.isNull() && filesArr.size() > 0) {
      for (JsonVariantConst f : filesArr) {
        const char* fn = f.as<const char*>();
        if (fn && *fn) {
          app.files.push_back(fn);
        }
      }
    }

    if (app.files.empty()) {
      app.files.push_back("manifest.json");
      app.files.push_back("main.lua");
    }

    outApps.push_back(std::move(app));
  }

  return true;
}

bool AppCatalog::validateSource(const std::string& repo, const std::string& branch, std::string& outCatalogName,
                                std::string& outError) {
  std::string normRepo = normalizeRepo(repo);
  if (normRepo.empty()) {
    outError = "Invalid repository format. Use owner/repo";
    return false;
  }

#if !defined(ESP32) && !defined(ARDUINO)
  outCatalogName = "Test Source";
  return true;
#else
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP) {
    outError = "Low memory for network transfer";
    return false;
  }

  std::vector<std::string> branchesToTry;
  std::string primaryBranch = branch.empty() ? "main" : branch;
  branchesToTry.push_back(primaryBranch);
  if (primaryBranch == "main") {
    branchesToTry.push_back("master");
  }

  Storage.mkdir("/.crosspoint");

  for (const auto& b : branchesToTry) {
    const char* filenames[] = {"catalog.json", "catalogue.json"};
    for (const char* fn : filenames) {
      std::string url = buildRawUrl(normRepo, b, fn, true);
      LOG_DBG("APPSRC", "Validating source with URL: %s", url.c_str());

      Storage.remove(TMP_VALIDATE_FILE);
      auto res = HttpDownloader::downloadToFile(url, TMP_VALIDATE_FILE, nullptr);
      if (res != HttpDownloader::OK) {
        Storage.remove(TMP_VALIDATE_FILE);
        continue;
      }

      HalFile file;
      if (!Storage.openFileForRead("APPSRC", TMP_VALIDATE_FILE, file)) {
        Storage.remove(TMP_VALIDATE_FILE);
        continue;
      }

      JsonDocument filter;
      filter["name"] = true;
      filter["apps"][0]["id"] = true;
      filter["apps"][0]["name"] = true;
      filter["apps"][0]["files"] = true;

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, file, DeserializationOption::Filter(filter));
      file.close();
      Storage.remove(TMP_VALIDATE_FILE);

      if (!err && (doc["apps"].is<JsonArrayConst>() || doc["name"].is<const char*>())) {
        const char* name = doc["name"] | "";
        outCatalogName = (name && *name) ? name : normRepo;
        LOG_INF("APPSRC", "Validated source '%s' (title: %s)", normRepo.c_str(), outCatalogName.c_str());
        return true;
      }
    }
  }

  outError = "No valid catalog.json found in repository";
  return false;
#endif
}

bool AppCatalog::fetchCatalog(const AppSource& source, std::vector<CatalogApp>& outApps, std::string& outError) {
  std::string normRepo = normalizeRepo(source.repo);
  if (normRepo.empty()) {
    outError = "Invalid repository format";
    return false;
  }

#if !defined(ESP32) && !defined(ARDUINO)
  outError = "Host mock: catalog fetching not supported";
  return false;
#else
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP) {
    outError = "Low memory for network transfer";
    return false;
  }

  Storage.mkdir("/.crosspoint");

  std::vector<std::string> branchesToTry;
  std::string primaryBranch = source.branch.empty() ? "main" : source.branch;
  branchesToTry.push_back(primaryBranch);
  if (primaryBranch == "main") {
    branchesToTry.push_back("master");
  }

  bool downloaded = false;
  for (const auto& b : branchesToTry) {
    const char* filenames[] = {"catalog.json", "catalogue.json"};
    for (const char* fn : filenames) {
      std::string url = buildRawUrl(normRepo, b, fn, true);
      Storage.remove(TMP_CATALOG_FILE);
      auto res = HttpDownloader::downloadToFile(url, TMP_CATALOG_FILE, nullptr);
      if (res == HttpDownloader::OK) {
        downloaded = true;
        break;
      }
      Storage.remove(TMP_CATALOG_FILE);
    }
    if (downloaded) break;
  }

  if (!downloaded) {
    outError = "Failed to download catalog from " + normRepo;
    return false;
  }

  HalFile file;
  if (!Storage.openFileForRead("APPSRC", TMP_CATALOG_FILE, file)) {
    Storage.remove(TMP_CATALOG_FILE);
    outError = "Failed to open downloaded catalog";
    return false;
  }

  // Parse directly from SD card stream
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  Storage.remove(TMP_CATALOG_FILE);

  if (err) {
    outError = std::string("Failed to parse catalog: ") + err.c_str();
    return false;
  }

  std::string catName;
  // Convert doc to string or parse directly from doc
  JsonArrayConst appsArr = doc["apps"].as<JsonArrayConst>();
  if (appsArr.isNull()) {
    outError = "Catalog missing 'apps' array";
    return false;
  }

  for (JsonObjectConst obj : appsArr) {
    const char* appId = obj["id"] | "";
    if (!appId || !*appId) continue;

    CatalogApp app;
    app.id = appId;
    app.name = obj["name"] | obj["title"] | appId;
    app.version = obj["version"] | "1.0.0";
    app.author = obj["author"] | "";
    app.description = obj["description"] | "";
    app.category = obj["category"] | "";
    app.icon = obj["icon"] | "Blocks";

    const char* p = obj["path"] | "";
    app.path = (p && *p) ? p : ("apps/" + app.id);

    app.sourceRepo = normRepo;
    app.sourceBranch = primaryBranch;

    JsonArrayConst filesArr = obj["files"].as<JsonArrayConst>();
    if (!filesArr.isNull() && filesArr.size() > 0) {
      for (JsonVariantConst f : filesArr) {
        const char* fn = f.as<const char*>();
        if (fn && *fn) {
          app.files.push_back(fn);
        }
      }
    }

    if (app.files.empty()) {
      app.files.push_back("manifest.json");
      app.files.push_back("main.lua");
    }

    outApps.push_back(std::move(app));
  }

  LOG_INF("APPSRC", "Loaded %zu apps from %s", outApps.size(), normRepo.c_str());
  return true;
#endif
}
