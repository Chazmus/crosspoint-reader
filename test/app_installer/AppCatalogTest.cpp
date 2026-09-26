#include <gtest/gtest.h>

#include "apps/installer/AppCatalog.h"
#include "apps/installer/AppSourceStore.h"

TEST(AppCatalogTest, NormalizeRepoVariants) {
  EXPECT_EQ(AppCatalog::normalizeRepo("chazmus/crosspoint-apps"), "chazmus/crosspoint-apps");
  EXPECT_EQ(AppCatalog::normalizeRepo("https://github.com/chazmus/crosspoint-apps"), "chazmus/crosspoint-apps");
  EXPECT_EQ(AppCatalog::normalizeRepo("https://github.com/chazmus/crosspoint-apps.git"), "chazmus/crosspoint-apps");
  EXPECT_EQ(AppCatalog::normalizeRepo("http://github.com/chazmus/crosspoint-apps/"), "chazmus/crosspoint-apps");
  EXPECT_EQ(AppCatalog::normalizeRepo("github.com/chazmus/crosspoint-apps"), "chazmus/crosspoint-apps");
  EXPECT_EQ(AppCatalog::normalizeRepo("  alice/my-retro-reader-games  "), "alice/my-retro-reader-games");

  // Invalid formats
  EXPECT_EQ(AppCatalog::normalizeRepo(""), "");
  EXPECT_EQ(AppCatalog::normalizeRepo("noslash"), "");
  EXPECT_EQ(AppCatalog::normalizeRepo("/onlyslash"), "");
  EXPECT_EQ(AppCatalog::normalizeRepo("trailing/"), "");
  EXPECT_EQ(AppCatalog::normalizeRepo("https://other-domain.com/user/repo"), "");
}

TEST(AppCatalogTest, BuildRawUrl) {
  EXPECT_EQ(AppCatalog::buildRawUrl("chazmus/crosspoint-apps", "main", "catalog.json"),
            "https://raw.githubusercontent.com/chazmus/crosspoint-apps/main/catalog.json");

  EXPECT_EQ(AppCatalog::buildRawUrl("chazmus/crosspoint-apps", "", "catalog.json"),
            "https://raw.githubusercontent.com/chazmus/crosspoint-apps/main/catalog.json");

  EXPECT_EQ(AppCatalog::buildRawUrl("user/repo", "dev", "apps/chess/main.lua"),
            "https://raw.githubusercontent.com/user/repo/dev/apps/chess/main.lua");

  EXPECT_EQ(AppCatalog::buildRawUrl("user/repo", "master", "/manifest.json"),
            "https://raw.githubusercontent.com/user/repo/master/manifest.json");

  std::string busted = AppCatalog::buildRawUrl("chazmus/crosspoint-apps", "main", "catalog.json", true);
  EXPECT_NE(busted.find("https://raw.githubusercontent.com/chazmus/crosspoint-apps/main/catalog.json?t="),
            std::string::npos);
}

TEST(AppCatalogTest, ParseCatalogJsonSuccess) {
  const char* json = R"({
    "name": "Community Apps",
    "description": "Verified Lua applications",
    "version": 1,
    "apps": [
      {
        "id": "chess",
        "name": "Daily Chess",
        "version": "1.2.0",
        "author": "Chazmus",
        "description": "Daily tactical puzzles",
        "icon": "Blocks",
        "path": "apps/chess",
        "files": ["manifest.json", "main.lua", "pieces.lua", "json.lua"]
      },
      {
        "id": "counter",
        "title": "Tally Counter",
        "version": "1.0.0",
        "author": "CrossPoint",
        "description": "Simple counter"
      }
    ]
  })";

  std::string catalogName;
  std::vector<CatalogApp> apps;
  std::string error;

  bool ok = AppCatalog::parseCatalogJson(json, "chazmus/crosspoint-apps", "main", catalogName, apps, error);
  ASSERT_TRUE(ok) << error;
  EXPECT_EQ(catalogName, "Community Apps");
  ASSERT_EQ(apps.size(), 2u);

  // App 1
  EXPECT_EQ(apps[0].id, "chess");
  EXPECT_EQ(apps[0].name, "Daily Chess");
  EXPECT_EQ(apps[0].version, "1.2.0");
  EXPECT_EQ(apps[0].author, "Chazmus");
  EXPECT_EQ(apps[0].description, "Daily tactical puzzles");
  EXPECT_EQ(apps[0].icon, "Blocks");
  EXPECT_EQ(apps[0].path, "apps/chess");
  EXPECT_EQ(apps[0].sourceRepo, "chazmus/crosspoint-apps");
  EXPECT_EQ(apps[0].sourceBranch, "main");
  ASSERT_EQ(apps[0].files.size(), 4u);
  EXPECT_EQ(apps[0].files[0], "manifest.json");
  EXPECT_EQ(apps[0].files[1], "main.lua");

  // App 2 (tests title alias, default path, and fallback files)
  EXPECT_EQ(apps[1].id, "counter");
  EXPECT_EQ(apps[1].name, "Tally Counter");
  EXPECT_EQ(apps[1].version, "1.0.0");
  EXPECT_EQ(apps[1].author, "CrossPoint");
  EXPECT_EQ(apps[1].path, "apps/counter");
  ASSERT_EQ(apps[1].files.size(), 2u);
  EXPECT_EQ(apps[1].files[0], "manifest.json");
  EXPECT_EQ(apps[1].files[1], "main.lua");
}

TEST(AppCatalogTest, ParseCatalogJsonErrors) {
  std::string catalogName;
  std::vector<CatalogApp> apps;
  std::string error;

  // Invalid JSON syntax
  EXPECT_FALSE(AppCatalog::parseCatalogJson("not a json", "user/repo", "main", catalogName, apps, error));

  // Missing apps array
  const char* noApps = R"({"name": "Test Repo"})";
  EXPECT_FALSE(AppCatalog::parseCatalogJson(noApps, "user/repo", "main", catalogName, apps, error));
  EXPECT_NE(error.find("apps"), std::string::npos);
}

TEST(AppSourceStoreTest, SerializationAndOperations) {
  JsonDocument doc;
  AppSourceStore& store = APP_SOURCE_STORE;

  // Clear and test default seeding
  while (store.getCount() > 0) {
    store.removeSource(0);
  }
  EXPECT_EQ(store.getCount(), 0u);

  store.seedDefaultsIfEmpty();
  EXPECT_EQ(store.getCount(), 1u);
  const auto* official = store.getSource(0);
  ASSERT_NE(official, nullptr);
  EXPECT_EQ(official->repo, "chazmus/crosspoint-apps");
  EXPECT_TRUE(official->enabled);

  // Add another source
  AppSource custom;
  custom.name = "My Retro Games";
  custom.repo = "alice/games";
  custom.branch = "master";
  custom.enabled = true;
  EXPECT_TRUE(store.addSource(custom));
  EXPECT_EQ(store.getCount(), 2u);

  // Prevent duplicate repo
  EXPECT_FALSE(store.addSource(custom));
  EXPECT_EQ(store.getCount(), 2u);

  // Toggle source
  EXPECT_TRUE(store.toggleSource(1));
  EXPECT_FALSE(store.getSource(1)->enabled);

  // Serialize to JSON
  store.toJson(doc);
  EXPECT_TRUE(doc["sources"].is<JsonArrayConst>());
  EXPECT_EQ(doc["sources"].as<JsonArrayConst>().size(), 2u);

  // Deserialize to fresh store
  EXPECT_TRUE(store.fromJson(doc.as<JsonVariantConst>()));
  EXPECT_EQ(store.getCount(), 2u);
  EXPECT_EQ(store.getSource(1)->repo, "alice/games");
  EXPECT_FALSE(store.getSource(1)->enabled);

  // Remove source
  EXPECT_TRUE(store.removeSource(1));
  EXPECT_EQ(store.getCount(), 1u);
  EXPECT_EQ(store.getSource(0)->repo, "chazmus/crosspoint-apps");
}
