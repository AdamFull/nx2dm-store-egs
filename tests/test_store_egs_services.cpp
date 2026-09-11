#include "framework/nxtest.h"

#include "store_egs/store_egs_platform.h"
#include "store_egs/store_egs_services.h"

// Unlike Steam's SteamAPI_Init() (a cheap local check), EOS's
// EgsPlatform::initialize() drives real network calls (EOS_Connect_Login
// talks to Epic's backend even for device-id auth) - not something to fire
// with fake credentials in a unit test. Every service method here checks
// EgsPlatform::has_product_user()/has_epic_account() before touching any
// EOS interface at all, so a never-initialized EgsPlatform (both always
// false) proves the same guard-path bar as store_steam's tests without
// ever making a real EOS call.

using namespace nxm::store_egs;

TEST_CASE("store_egs services: a never-initialized platform reports "
          "correctly") {
  const EgsPlatform platform;
  CHECK_FALSE(platform.ready());
  CHECK_FALSE(platform.has_product_user());
  CHECK_FALSE(platform.has_epic_account());
}

TEST_CASE("store_egs services: EgsCore refuses safely with no platform") {
  EgsPlatform platform;
  EgsCore core(platform);
  CHECK_FALSE(core.is_owned());
  CHECK_FALSE(core.is_owned("some_dlc"));
  CHECK(core.owned_dlc_ids().empty());
  CHECK(core.store_name() == "egs");
  core.refresh_entitlements();
  CHECK(core.owned_dlc_ids().empty());
}

TEST_CASE("store_egs services: EgsIap refuses safely with no platform") {
  EgsPlatform platform;
  EgsIap iap(platform);
  CHECK(iap.products().empty());
  CHECK_FALSE(iap.purchase("some_offer"));
  CHECK_FALSE(iap.purchase_pending());
  CHECK(iap.purchase_error().empty());
  iap.refresh_offers();
  CHECK(iap.products().empty());
}

TEST_CASE(
    "store_egs services: EgsAchievements refuses safely with no platform") {
  EgsPlatform platform;
  EgsAchievements achievements(platform);
  CHECK_FALSE(achievements.unlock("first_win"));
  CHECK_FALSE(achievements.is_unlocked("first_win"));
  CHECK(achievements.achievement_ids().empty());
  CHECK_FALSE(achievements.set_stat("enemies_killed", 5.0));
  CHECK(achievements.stat("enemies_killed") == 0.0);
  achievements.refresh_definitions();
  achievements.refresh_player_achievements();
  achievements.refresh_stats();
  CHECK(achievements.achievement_ids().empty());
}

TEST_CASE(
    "store_egs services: EgsCloudSaves refuses safely with no platform") {
  EgsPlatform platform;
  EgsCloudSaves saves(platform);
  CHECK_FALSE(saves.write("slot1", "data"));
  CHECK(saves.read("slot1").empty());
  CHECK_FALSE(saves.exists("slot1"));
  CHECK_FALSE(saves.remove("slot1"));
  CHECK(saves.keys().empty());
  CHECK(saves.bytes_used() == 0u);
  CHECK(saves.bytes_total() == 0u);
  saves.refresh_keys();
  CHECK(saves.keys().empty());
}

TEST_CASE(
    "store_egs services: EgsPresence refuses safely with no platform") {
  EgsPlatform platform;
  EgsPresence presence(platform);
  CHECK_FALSE(presence.set_status("in menu"));
  CHECK(presence.own_name().empty());
  CHECK(presence.friend_count() == 0u);
  CHECK(presence.friend_names().empty());
  presence.refresh_own_name();
  presence.refresh_friends();
  CHECK(presence.friend_names().empty());
}
