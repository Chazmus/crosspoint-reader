#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct AppSource {
  std::string name;
  std::string repo;    // owner/repo (e.g. "chazmus/crosspoint-apps")
  std::string branch;  // default "main"
  bool enabled = true;
};

class AppSourceStore : public PersistableStore<AppSourceStore> {
 private:
  std::vector<AppSource> sources_;
  static constexpr size_t MAX_SOURCES = 16;

  AppSourceStore() = default;
  friend class PersistableStore<AppSourceStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/app_sources.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool addSource(const AppSource& source);
  bool updateSource(size_t index, const AppSource& source);
  bool toggleSource(size_t index);
  bool removeSource(size_t index);

  const std::vector<AppSource>& getSources() const { return sources_; }
  const AppSource* getSource(size_t index) const;
  size_t getCount() const { return sources_.size(); }
  bool hasSources() const { return !sources_.empty(); }

  void seedDefaultsIfEmpty();
};

#define APP_SOURCE_STORE AppSourceStore::getInstance()
