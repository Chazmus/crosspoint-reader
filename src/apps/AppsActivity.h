#pragma once

#include <vector>

#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

class AppsActivity final : public UiListActivity {
 public:
  AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  void rebuildRows();

  std::vector<freeink::ui::ListItem> rowItems_;
};
