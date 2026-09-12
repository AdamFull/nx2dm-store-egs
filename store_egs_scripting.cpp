#include "store_egs/store_egs_scripting.h"

#include "store_egs/store_egs_leaderboards.h"
#include "store_egs/store_egs_mods.h"
#include "store_egs/store_egs_overlay.h"

#include "core/script/script_host.h"

namespace nxm::store_egs {

void expose_store_egs_extras(nxe::script::Host &host,
                             EgsLeaderboards &leaderboards, EgsOverlay &overlay,
                             EgsMods &mods) {
  // -- Leaderboards ---------------------------------------------------

  host.expose_as("store_egs_leaderboard_download",
                 [&leaderboards](const nx::string_view leaderboard_id) {
                   return leaderboards.download(leaderboard_id);
                 });
  host.expose_as("store_egs_leaderboard_download_pending", [&leaderboards]() {
    return leaderboards.download_pending();
  });
  host.expose_as("store_egs_leaderboard_entry_count", [&leaderboards]() {
    return static_cast<f64>(leaderboards.entry_count());
  });
  host.expose_as("store_egs_leaderboard_entry_rank",
                 [&leaderboards](const f64 index) {
                   return static_cast<f64>(
                       leaderboards.entry_rank(nx::cast<usize>(index)));
                 });
  host.expose_as("store_egs_leaderboard_entry_score",
                 [&leaderboards](const f64 index) {
                   return static_cast<f64>(
                       leaderboards.entry_score(nx::cast<usize>(index)));
                 });
  host.expose_as("store_egs_leaderboard_entry_name",
                 [&leaderboards](const f64 index) {
                   return leaderboards.entry_name(nx::cast<usize>(index));
                 });

  // -- Overlay ----------------------------------------------------------

  host.expose_as("store_egs_overlay_show_friends",
                 [&overlay]() { return overlay.show_friends(); });
  host.expose_as("store_egs_overlay_hide_friends",
                 [&overlay]() { return overlay.hide_friends(); });
  host.expose_as("store_egs_overlay_friends_visible",
                 [&overlay]() { return overlay.friends_visible(); });
  host.expose_as("store_egs_overlay_show_block_player",
                 [&overlay](const f64 friend_index) {
                   return overlay.show_block_player(nx::cast<usize>(friend_index));
                 });
  host.expose_as("store_egs_overlay_show_report_player",
                 [&overlay](const f64 friend_index) {
                   return overlay.show_report_player(nx::cast<usize>(friend_index));
                 });
  host.expose_as("store_egs_overlay_show_native_profile",
                 [&overlay](const f64 friend_index) {
                   return overlay.show_native_profile(nx::cast<usize>(friend_index));
                 });
  host.expose_as("store_egs_overlay_pause_social_overlay",
                 [&overlay](const bool paused) {
                   return overlay.pause_social_overlay(paused);
                 });
  host.expose_as("store_egs_overlay_social_overlay_paused",
                 [&overlay]() { return overlay.social_overlay_paused(); });

  // -- Mods -------------------------------------------------------------

  host.expose_as("store_egs_mods_refresh_installed",
                 [&mods]() { mods.refresh_installed(); return true; });
  host.expose_as("store_egs_mods_refresh_available",
                 [&mods]() { mods.refresh_available(); return true; });

  host.expose_as("store_egs_mods_installed_count", [&mods]() {
    return static_cast<f64>(mods.installed_count());
  });
  host.expose_as("store_egs_mods_installed_title",
                 [&mods](const f64 index) {
                   return mods.installed_title(nx::cast<usize>(index));
                 });
  host.expose_as("store_egs_mods_installed_version",
                 [&mods](const f64 index) {
                   return mods.installed_version(nx::cast<usize>(index));
                 });

  host.expose_as("store_egs_mods_available_count", [&mods]() {
    return static_cast<f64>(mods.available_count());
  });
  host.expose_as("store_egs_mods_available_title",
                 [&mods](const f64 index) {
                   return mods.available_title(nx::cast<usize>(index));
                 });
  host.expose_as("store_egs_mods_available_version",
                 [&mods](const f64 index) {
                   return mods.available_version(nx::cast<usize>(index));
                 });

  host.expose_as("store_egs_mods_install", [&mods](const f64 available_index) {
    return mods.install(nx::cast<usize>(available_index));
  });
  host.expose_as("store_egs_mods_install_pending",
                 [&mods]() { return mods.install_pending(); });
  host.expose_as("store_egs_mods_install_error",
                 [&mods]() -> nx::string_view { return mods.install_error(); });

  host.expose_as("store_egs_mods_uninstall",
                 [&mods](const f64 installed_index) {
                   return mods.uninstall(nx::cast<usize>(installed_index));
                 });
  host.expose_as("store_egs_mods_uninstall_pending",
                 [&mods]() { return mods.uninstall_pending(); });
  host.expose_as("store_egs_mods_uninstall_error", [&mods]() -> nx::string_view {
    return mods.uninstall_error();
  });

  host.expose_as("store_egs_mods_update", [&mods](const f64 installed_index) {
    return mods.update(nx::cast<usize>(installed_index));
  });
  host.expose_as("store_egs_mods_update_pending",
                 [&mods]() { return mods.update_pending(); });
  host.expose_as("store_egs_mods_update_error",
                 [&mods]() -> nx::string_view { return mods.update_error(); });
}

}
