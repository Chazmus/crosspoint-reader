#pragma once

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "AppSourceStore.h"

struct CatalogApp {
  std::string id;
  std::string name;
  std::string version;
  std::string author;
  std::string description;
  std::string category;
  std::string icon = "Blocks";
  std::string path;  // relative folder in repo, defaults to "apps/<id>"
  std::vector<std::string> files;
  std::string sourceRepo;
  std::string sourceBranch;

  bool isInstalled() const;
  std::string getInstalledVersion() const;
  bool hasUpdate() const;
};

class AppCatalog {
 public:
  static std::string normalizeRepo(const std::string& input);
  static std::string buildRawUrl(const std::string& repo, const std::string& branch, const std::string& path);

  static bool validateSource(const std::string& repo, const std::string& branch, std::string& outCatalogName,
                             std::string& outError);

  static bool fetchCatalog(const AppSource& source, std::vector<CatalogApp>& outApps, std::string& outError);

  static bool parseCatalogJson(const char* jsonContent, const std::string& repo, const std::string& branch,
                               std::string& outCatalogName, std::vector<CatalogApp>& outApps, std::string& outError);
};
