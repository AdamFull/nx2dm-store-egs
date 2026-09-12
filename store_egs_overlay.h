#pragma once

#include "store_egs/store_egs_platform.h"
#include "store_egs/store_egs_services.h"

#include "core/foundation/core/foundation.h"

#include <eos_ui.h>

namespace nxm::store_egs {

/// EOS's social-overlay control (EOS_HUI) - an EGS-only extra, the closest
/// analog to store_steam's SteamOverlay. Uses the Epic-account auth group
/// (same as EgsPresence/friends), not the device-id one - guards on
/// EgsPlatform::has_epic_account().
///
/// ShowBlockPlayer/ShowReportPlayer/ShowNativeProfile all need a
/// TargetUserId - like store_steam's own open_to_friend(friend_index,
/// dialog), this takes a friends-list index and resolves it internally via
/// EgsPresence::friend_id_at() rather than exposing a raw EOS_EpicAccountId
/// Luau has no safe way to carry.
///
/// The three async calls below don't track pending/result state the way
/// EgsLeaderboards' download() does - their completion callback exists only
/// to log a failed ResultCode, matching store_steam's own
/// ActivateGameOverlay()/ActivateGameOverlayToWebPage() being pure
/// fire-and-forget from Luau's perspective.
class EgsOverlay {
public:
  EgsOverlay(EgsPlatform &platform, EgsPresence &presence) noexcept
      : m_platform(platform), m_presence(presence) {}

  bool show_friends() const;
  bool hide_friends() const;
  [[nodiscard]] bool friends_visible() const;

  bool show_block_player(usize friend_index) const;
  bool show_report_player(usize friend_index) const;
  bool show_native_profile(usize friend_index) const;

  bool pause_social_overlay(bool paused) const;
  [[nodiscard]] bool social_overlay_paused() const;

private:
  static void EOS_CALL
  show_friends_callback(const EOS_UI_ShowFriendsCallbackInfo *data);
  static void EOS_CALL
  hide_friends_callback(const EOS_UI_HideFriendsCallbackInfo *data);
  static void EOS_CALL
  show_block_player_callback(const EOS_UI_OnShowBlockPlayerCallbackInfo *data);
  static void EOS_CALL show_report_player_callback(
      const EOS_UI_OnShowReportPlayerCallbackInfo *data);
  static void EOS_CALL
  show_native_profile_callback(const EOS_UI_ShowNativeProfileCallbackInfo *data);

  EgsPlatform &m_platform;
  EgsPresence &m_presence;
};

}
