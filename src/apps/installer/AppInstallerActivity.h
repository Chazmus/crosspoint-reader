#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AppCatalog.h"
#include "AppInstaller.h"
#include "AppSourceStore.h"
#include "activities/UiTabListActivity.h"

class AppInstallerActivity final : public UiTabListActivity {
 public:
  explicit AppInstallerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return state_ != State::LIST; }
  bool skipLoopDelay() override { return state_ == State::DOWNLOADING; }

 protected:
  const char* headerTitle() const override;
  int tabCount() const override { return 2; }
  int activeTab() const override { return currentTab_; }
  const char* tabLabel(int index) const override { return index == 0 ? "Apps" : "Sources"; }
  void onTabAction(int index) override;
  void stepTab(int direction) override;
  bool handleButtons() override;
  void onBackButton() override;
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowLongPress(int index) override;
  bool handleCustomInput() override;

 private:
  enum class State { WIFI_CONNECTING, LOADING_CATALOGS, LIST, VALIDATING_SOURCE, DOWNLOADING, COMPLETE, ERROR };

  State state_ = State::WIFI_CONNECTING;
  int currentTab_ = 0;
  bool wifiStartedByUs_ = false;

  std::vector<CatalogApp> apps_;
  std::vector<freeink::ui::ListItem> rowItems_;
  std::vector<std::string> rowSubtitles_;
  std::vector<std::string> rowValues_;

  // Download / operation tracking
  int selectedAppIndex_ = -1;
  size_t currentFileIndex_ = 0;
  size_t currentFileTotal_ = 0;
  size_t fileProgress_ = 0;
  size_t fileTotal_ = 0;
  std::string currentFileName_;
  std::string statusMessage_;
  std::string errorMessage_;
  bool cancelRequested_ = false;

  void onWifiSelectionComplete(bool success);
  void refreshCatalogs();
  void rebuildRowItems();
  void rebuildAppsRows();
  void rebuildSourcesRows();
  void startAppInstall(const CatalogApp& app);
  void promptAppUninstall(const CatalogApp& app);
  void promptDeleteSource(size_t sourceIndex);
  void onAddSourceResult(const ActivityResult& result);
};
