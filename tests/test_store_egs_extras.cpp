#include "framework/nxtest.h"

#include "store_egs/store_egs_leaderboards.h"
#include "store_egs/store_egs_mods.h"
#include "store_egs/store_egs_overlay.h"
#include "store_egs/store_egs_services.h"

// Same guard-path bar as test_store_egs_services.cpp: no live EOS session
// runs here, so has_product_user()/has_epic_account() are both false
// throughout this binary and every operation below must refuse safely.

using namespace nxm::store_egs;

TEST_CASE("store_egs extras: Leaderboards refuses safely with no session") {
  EgsPlatform platform;
  EgsLeaderboards leaderboards(platform);
  CHECK_FALSE(leaderboards.download("high_scores"));
  CHECK_FALSE(leaderboards.download_pending());
  CHECK(leaderboards.entry_count() == 0u);
  CHECK(leaderboards.entry_rank(0) == 0u);
  CHECK(leaderboards.entry_score(0) == 0);
  CHECK(leaderboards.entry_name(0).empty());
}

TEST_CASE("store_egs extras: Overlay refuses safely with no session") {
  EgsPlatform platform;
  EgsPresence presence(platform);
  const EgsOverlay overlay(platform, presence);
  CHECK_FALSE(overlay.show_friends());
  CHECK_FALSE(overlay.hide_friends());
  CHECK_FALSE(overlay.friends_visible());
  CHECK_FALSE(overlay.show_block_player(0));
  CHECK_FALSE(overlay.show_report_player(0));
  CHECK_FALSE(overlay.show_native_profile(0));
  // Neither of these needs a signed-in account, but they still need a real
  // platform handle - never initialized here either.
  CHECK_FALSE(overlay.pause_social_overlay(true));
  CHECK_FALSE(overlay.social_overlay_paused());
}

TEST_CASE("store_egs extras: Mods refuses safely with no session") {
  EgsPlatform platform;
  EgsMods mods(platform);
  mods.refresh_installed();
  mods.refresh_available();
  CHECK(mods.installed_count() == 0u);
  CHECK(mods.installed_title(0).empty());
  CHECK(mods.installed_version(0).empty());
  CHECK(mods.available_count() == 0u);
  CHECK(mods.available_title(0).empty());
  CHECK(mods.available_version(0).empty());

  CHECK_FALSE(mods.install(0));
  CHECK_FALSE(mods.install_pending());
  CHECK(mods.install_error().empty());

  CHECK_FALSE(mods.uninstall(0));
  CHECK_FALSE(mods.uninstall_pending());
  CHECK(mods.uninstall_error().empty());

  CHECK_FALSE(mods.update(0));
  CHECK_FALSE(mods.update_pending());
  CHECK(mods.update_error().empty());
}
