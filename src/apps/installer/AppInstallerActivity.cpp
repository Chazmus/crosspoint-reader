#include "AppInstallerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

#if defined(ESP32) || defined(ARDUINO)
#include <WiFi.h>
#endif

namespace fui = freeink::ui;

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
}  // namespace

AppInstallerActivity::AppInstallerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("AppInstaller", renderer, mappedInput, true) {}

void AppInstallerActivity::onEnter() {
  APP_SOURCE_STORE.loadFromFile();
  APP_SOURCE_STORE.seedDefaultsIfEmpty();

  UiTabListActivity::onEnter();

#if defined(ESP32) || defined(ARDUINO)
  if (WiFi.status() == WL_CONNECTED) {
    onWifiSelectionComplete(true);
  } else {
    wifiStartedByUs_ = true;
    WiFi.mode(WIFI_STA);
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                           [this](const ActivityResult& res) { onWifiSelectionComplete(!res.isCancelled); });
  }
#else
  onWifiSelectionComplete(true);
#endif
}

void AppInstallerActivity::onExit() {
  UiTabListActivity::onExit();

#if defined(ESP32) || defined(ARDUINO)
  if (wifiStartedByUs_ && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
  }
#endif
}

void AppInstallerActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    finish();
    return;
  }

  {
    RenderLock lock(*this);
    state_ = State::LOADING_CATALOGS;
  }
  requestUpdateAndWait();

  refreshCatalogs();

  {
    RenderLock lock(*this);
    state_ = State::LIST;
    rebuildRowItems();
  }
  requestUpdate();
}

void AppInstallerActivity::refreshCatalogs() {
  apps_.clear();
  const auto& sources = APP_SOURCE_STORE.getSources();
  for (const auto& src : sources) {
    if (!src.enabled) continue;
    std::string err;
    AppCatalog::fetchCatalog(src, apps_, err);
  }
}

void AppInstallerActivity::rebuildRowItems() {
  if (currentTab_ == 0) {
    rebuildAppsRows();
  } else {
    rebuildSourcesRows();
  }
}

void AppInstallerActivity::rebuildAppsRows() {
  rowItems_.clear();
  rowSubtitles_.clear();
  rowValues_.clear();

  rowItems_.reserve(apps_.size());
  rowSubtitles_.reserve(apps_.size());
  rowValues_.reserve(apps_.size());

  for (size_t i = 0; i < apps_.size(); ++i) {
    const auto& app = apps_[i];
    fui::ListItem item;
    item.label = app.name.c_str();

    std::string sub = app.author.empty() ? "" : (app.author + " • ");
    if (app.hasUpdate()) {
      sub += "Update: v" + app.getInstalledVersion() + " -> v" + app.version;
    } else if (app.isInstalled()) {
      sub += "v" + app.getInstalledVersion() + " (Installed)";
    } else {
      sub += "v" + app.version;
    }
    rowSubtitles_.push_back(std::move(sub));
    item.subtitle = rowSubtitles_.back().c_str();

    if (app.hasUpdate()) {
      rowValues_.push_back("Update");
    } else if (app.isInstalled()) {
      rowValues_.push_back("Installed");
    } else {
      rowValues_.push_back("Install");
    }
    item.value = rowValues_.back().c_str();

    item.icon = listIconFor(parseIcon(app.icon.c_str()), 32);
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }
}

void AppInstallerActivity::rebuildSourcesRows() {
  rowItems_.clear();
  rowSubtitles_.clear();
  rowValues_.clear();

  const auto& sources = APP_SOURCE_STORE.getSources();
  rowItems_.reserve(sources.size() + 1);
  rowSubtitles_.reserve(sources.size() + 1);
  rowValues_.reserve(sources.size() + 1);

  for (size_t i = 0; i < sources.size(); ++i) {
    const auto& src = sources[i];
    fui::ListItem item;
    item.label = src.name.c_str();

    std::string sub = src.repo + " (" + src.branch + ")";
    rowSubtitles_.push_back(std::move(sub));
    item.subtitle = rowSubtitles_.back().c_str();

    rowValues_.push_back(src.enabled ? "Active" : "Disabled");
    item.value = rowValues_.back().c_str();

    item.icon = listIconFor(UIIcon::Library, 32);
    item.actionValue = static_cast<int16_t>(i);
    rowItems_.push_back(item);
  }

  fui::ListItem addItem;
  addItem.label = "+ Add Repository...";
  rowSubtitles_.push_back("Add a public GitHub repo (owner/repo)");
  addItem.subtitle = rowSubtitles_.back().c_str();
  addItem.icon = listIconFor(UIIcon::Transfer, 32);
  addItem.actionValue = static_cast<int16_t>(sources.size());
  rowItems_.push_back(addItem);
}

int AppInstallerActivity::listCount() const { return static_cast<int>(rowItems_.size()); }

void AppInstallerActivity::onTabAction(const int index) {
  if (index != currentTab_) {
    currentTab_ = index;
    app.clearTapFlash();
    rebuildRowItems();
    requestUpdate();
  }
}

void AppInstallerActivity::stepTab(const int direction) {
  currentTab_ = (currentTab_ + direction + 2) % 2;
  app.clearTapFlash();
  rebuildRowItems();
  requestUpdate();
}

bool AppInstallerActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (ringPos() == 0) {
      stepTab(1);
    } else {
      activateIndex(ringPos() - 1);
    }
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (ringPos() > 0) {
      activeNav().selected = 0;
      requestUpdate();
    } else {
      onBackButton();
    }
    return true;
  }
  return false;
}

void AppInstallerActivity::onBackButton() { finish(); }

void AppInstallerActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  buildTabBar(screen);

  if (rowItems_.empty()) {
    screen.spacer(static_cast<int16_t>(metrics.verticalSpacing * 2));
    screen.centeredText(currentTab_ == 0 ? "No apps available" : "No sources configured", screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 2;
  syncTabListViewport(screen, props);
  screen.list(props);
}

void AppInstallerActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (currentTab_ == 0) {
    if (index < 0 || index >= static_cast<int>(apps_.size())) return;
    const auto& app = apps_[index];
    if (!app.isInstalled() || app.hasUpdate()) {
      startAppInstall(app);
    } else {
      promptAppUninstall(app);
    }
  } else {
    const auto& sources = APP_SOURCE_STORE.getSources();
    if (index == static_cast<int>(sources.size())) {
      startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput,
                                                                    "GitHub Repo (owner/repo):", "", 64,
                                                                    InputType::Text),
                             [this](const ActivityResult& res) { onAddSourceResult(res); });
    } else if (index >= 0 && index < static_cast<int>(sources.size())) {
      APP_SOURCE_STORE.toggleSource(static_cast<size_t>(index));
      rebuildRowItems();
      requestUpdate();
    }
  }
}

void AppInstallerActivity::onRowLongPress(const int index) {
  app.clearTapFlash();
  if (currentTab_ == 0) {
    if (index < 0 || index >= static_cast<int>(apps_.size())) return;
    const auto& app = apps_[index];
    if (app.isInstalled()) {
      promptAppUninstall(app);
    }
  } else {
    const auto& sources = APP_SOURCE_STORE.getSources();
    if (index >= 0 && index < static_cast<int>(sources.size())) {
      promptDeleteSource(static_cast<size_t>(index));
    }
  }
}

void AppInstallerActivity::startAppInstall(const CatalogApp& app) {
  selectedAppIndex_ = -1;
  for (size_t i = 0; i < apps_.size(); ++i) {
    if (apps_[i].id == app.id) {
      selectedAppIndex_ = static_cast<int>(i);
      break;
    }
  }

  cancelRequested_ = false;
  currentFileIndex_ = 0;
  currentFileTotal_ = app.files.size();
  fileProgress_ = 0;
  fileTotal_ = 0;
  currentFileName_ = "";

  {
    RenderLock lock(*this);
    state_ = State::DOWNLOADING;
  }
  requestUpdateAndWait();

  std::string err;
  bool ok = AppInstaller::installApp(
      app,
      [this](size_t fileIdx, size_t fileCount, size_t fileDown, size_t fileTot) {
        currentFileIndex_ = fileIdx;
        currentFileTotal_ = fileCount;
        fileProgress_ = fileDown;
        fileTotal_ = fileTot;
        requestUpdate();
      },
      &cancelRequested_, err);

  {
    RenderLock lock(*this);
    if (ok) {
      state_ = State::COMPLETE;
    } else {
      state_ = State::ERROR;
      errorMessage_ = err.empty() ? "Installation failed" : err;
    }
  }
  requestUpdate();
}

void AppInstallerActivity::promptAppUninstall(const CatalogApp& app) {
  std::string heading = "Uninstall " + app.name + "?";
  std::string body = "Remove /apps/" + app.id + " from SD card";
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, body),
      [this, appId = app.id](const ActivityResult& result) {
        if (!result.isCancelled) {
          std::string err;
          AppInstaller::uninstallApp(appId, err);
          rebuildRowItems();
          requestUpdate();
        }
      });
}

void AppInstallerActivity::promptDeleteSource(const size_t sourceIndex) {
  const auto* src = APP_SOURCE_STORE.getSource(sourceIndex);
  if (!src) return;
  std::string heading = "Remove Repository?";
  std::string body = src->name + " (" + src->repo + ")";
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, body),
      [this, sourceIndex](const ActivityResult& result) {
        if (!result.isCancelled) {
          APP_SOURCE_STORE.removeSource(sourceIndex);
          rebuildRowItems();
          requestUpdate();
        }
      });
}

void AppInstallerActivity::onAddSourceResult(const ActivityResult& result) {
  if (result.isCancelled) return;
  std::string repo = std::get<KeyboardResult>(result.data).text;
  std::string norm = AppCatalog::normalizeRepo(repo);
  if (norm.empty()) {
    {
      RenderLock lock(*this);
      state_ = State::ERROR;
      errorMessage_ = "Invalid repo format. Use owner/repo";
    }
    requestUpdate();
    return;
  }

  {
    RenderLock lock(*this);
    state_ = State::VALIDATING_SOURCE;
    statusMessage_ = "Validating " + norm + "...";
  }
  requestUpdateAndWait();

  std::string catName;
  std::string err;
  bool valid = AppCatalog::validateSource(norm, "main", catName, err);

  if (valid) {
    AppSource newSrc;
    newSrc.name = catName;
    newSrc.repo = norm;
    newSrc.branch = "main";
    newSrc.enabled = true;
    APP_SOURCE_STORE.addSource(newSrc);

    refreshCatalogs();

    {
      RenderLock lock(*this);
      state_ = State::LIST;
      rebuildRowItems();
    }
  } else {
    {
      RenderLock lock(*this);
      state_ = State::ERROR;
      errorMessage_ = err.empty() ? "catalog.json not found" : err;
    }
  }
  requestUpdate();
}

bool AppInstallerActivity::handleCustomInput() {
  if (state_ == State::LIST) {
    return false;
  }

  if (state_ == State::DOWNLOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      cancelRequested_ = true;
    }
    return true;
  }

  if (state_ == State::COMPLETE || state_ == State::ERROR) {
    int x = 0;
    int y = 0;
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
      {
        RenderLock lock(*this);
        state_ = State::LIST;
        rebuildRowItems();
      }
      requestUpdate();
    }
    return true;
  }

  return true;
}

void AppInstallerActivity::render(RenderLock&& lock) {
  if (state_ == State::LIST) {
    UiTabListActivity::render(std::move(lock));
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const auto centerY = (pageHeight - lineHeight) / 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "App Store",
                 currentTab_ == 0 ? "Browse Applications" : "Repository Sources");

  if (state_ == State::WIFI_CONNECTING) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, "Connecting to Wi-Fi...");
  } else if (state_ == State::LOADING_CATALOGS) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, "Fetching app catalogs...");
  } else if (state_ == State::VALIDATING_SOURCE) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY,
                              statusMessage_.empty() ? "Validating repository..." : statusMessage_.c_str());
  } else if (state_ == State::DOWNLOADING) {
    if (selectedAppIndex_ >= 0 && selectedAppIndex_ < static_cast<int>(apps_.size())) {
      const auto& app = apps_[selectedAppIndex_];
      std::string text = "Installing " + app.name;
      if (currentFileTotal_ > 0) {
        text += " (" + std::to_string(currentFileIndex_ + 1) + "/" + std::to_string(currentFileTotal_) + ")";
      }
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, text.c_str());
    }
    float progress = 0.0f;
    if (fileTotal_ > 0) {
      progress = static_cast<float>(fileProgress_) / static_cast<float>(fileTotal_);
    }
    int barY = centerY + metrics.verticalSpacing;
    GUI.drawProgressBar(
        renderer,
        Rect{metrics.contentSidePadding, barY, pageWidth - metrics.contentSidePadding * 2, metrics.progressBarHeight},
        static_cast<int>(progress * 100), 100);

    const auto labels = mappedInput.mapLabels("Cancel", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state_ == State::COMPLETE) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, "Installation Complete!", true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels("Back", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state_ == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, "Operation Failed", true, EpdFontFamily::BOLD);
    if (!errorMessage_.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + metrics.verticalSpacing, errorMessage_.c_str());
    }
    const auto labels = mappedInput.mapLabels("Back", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
