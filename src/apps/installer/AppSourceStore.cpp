#include "AppSourceStore.h"

#include <Logging.h>

#include <algorithm>

void AppSourceStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["sources"].to<JsonArray>();
  for (const auto& src : sources_) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = src.name;
    obj["repo"] = src.repo;
    obj["branch"] = src.branch.empty() ? "main" : src.branch;
    obj["enabled"] = src.enabled;
  }
}

bool AppSourceStore::fromJson(JsonVariantConst doc) {
  sources_.clear();
  JsonArrayConst arr = doc["sources"].as<JsonArrayConst>();
  sources_.reserve(std::min(arr.size(), MAX_SOURCES));

  for (JsonObjectConst obj : arr) {
    if (sources_.size() >= MAX_SOURCES) break;
    AppSource src;
    src.name = obj["name"] | "";
    src.repo = obj["repo"] | "";
    src.branch = obj["branch"] | "main";
    src.enabled = obj["enabled"] | true;
    if (!src.repo.empty()) {
      sources_.push_back(std::move(src));
    }
  }

  seedDefaultsIfEmpty();
  LOG_DBG("APPSRC", "Loaded %zu app sources", sources_.size());
  return true;
}

void AppSourceStore::seedDefaultsIfEmpty() {
  if (sources_.empty()) {
    AppSource official;
    official.name = "CrossPoint Official Apps";
    official.repo = "chazmus/crosspoint-apps";
    official.branch = "main";
    official.enabled = true;
    sources_.push_back(std::move(official));
    saveToFile();
  }
}

bool AppSourceStore::addSource(const AppSource& source) {
  if (sources_.size() >= MAX_SOURCES) {
    LOG_DBG("APPSRC", "Cannot add source, limit reached: %zu", MAX_SOURCES);
    return false;
  }
  for (const auto& s : sources_) {
    if (s.repo == source.repo) {
      LOG_DBG("APPSRC", "Source already exists: %s", source.repo.c_str());
      return false;
    }
  }
  sources_.push_back(source);
  return saveToFile();
}

bool AppSourceStore::updateSource(size_t index, const AppSource& source) {
  if (index >= sources_.size()) return false;
  sources_[index] = source;
  return saveToFile();
}

bool AppSourceStore::toggleSource(size_t index) {
  if (index >= sources_.size()) return false;
  sources_[index].enabled = !sources_[index].enabled;
  return saveToFile();
}

bool AppSourceStore::removeSource(size_t index) {
  if (index >= sources_.size()) return false;
  sources_.erase(sources_.begin() + index);
  return saveToFile();
}

const AppSource* AppSourceStore::getSource(size_t index) const {
  if (index >= sources_.size()) return nullptr;
  return &sources_[index];
}
