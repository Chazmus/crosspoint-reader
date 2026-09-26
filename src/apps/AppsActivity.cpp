#include "AppsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "apps/installer/AppInstallerActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

AppsActivity::AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("Apps", renderer, mappedInput) {
  rebuildRows();
}

void AppsActivity::onEnter() {
  UiListActivity::onEnter();
  rebuildRows();
}

void AppsActivity::rebuildRows() {
  AppRegistry::refreshApps();
  const auto& apps = AppRegistry::getApps();

  rowItems_.clear();
  rowItems_.reserve(apps.size() + 1);

  // Top item: App Store / Community Package Manager
  fui::ListItem storeItem;
  storeItem.label = "App Store";
  storeItem.subtitle = "Download community Lua apps & games";
  storeItem.icon = listIconFor(UIIcon::Transfer, 32);
  storeItem.actionValue = 0;
  rowItems_.push_back(storeItem);

  for (size_t i = 0; i < apps.size(); ++i) {
    fui::ListItem item;
    item.label = apps[i].resolveTitle();
    item.subtitle = apps[i].resolveDescription();
    item.icon = listIconFor(apps[i].icon, 32);
    item.actionValue = static_cast<int16_t>(i + 1);
    rowItems_.push_back(item);
  }
}

int AppsActivity::listCount() const { return static_cast<int>(rowItems_.size()); }

const char* AppsActivity::headerTitle() const { return "Applications"; }

void AppsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  nav.selected = index;

  if (index == 0) {
    // Launch App Store
    activityManager.pushActivity(std::make_unique<AppInstallerActivity>(renderer, mappedInput));
    return;
  }

  const size_t appIdx = static_cast<size_t>(index - 1);
  const auto* appDesc = AppRegistry::getAppAt(appIdx);
  if (appDesc && appDesc->createInstance) {
    auto activity = appDesc->createInstance(*appDesc, renderer, mappedInput);
    if (activity) {
      activityManager.pushActivity(std::move(activity));
    }
  }
}

void AppsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
