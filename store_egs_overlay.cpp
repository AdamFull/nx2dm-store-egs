#include "store_egs/store_egs_overlay.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_egs {
namespace {

const nx::log::Category log_store_egs = nx::log::category("store_egs");

}

bool EgsOverlay::show_friends() const {
  if (!m_platform.has_epic_account())
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_ShowFriendsOptions options{};
  options.ApiVersion = EOS_UI_SHOWFRIENDS_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();

  EOS_UI_ShowFriends(ui, &options, nullptr, &EgsOverlay::show_friends_callback);
  return true;
}

void EOS_CALL
EgsOverlay::show_friends_callback(const EOS_UI_ShowFriendsCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_UI_ShowFriends failed: {}",
              static_cast<i32>(data->ResultCode));
}

bool EgsOverlay::hide_friends() const {
  if (!m_platform.has_epic_account())
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_HideFriendsOptions options{};
  options.ApiVersion = EOS_UI_HIDEFRIENDS_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();

  EOS_UI_HideFriends(ui, &options, nullptr, &EgsOverlay::hide_friends_callback);
  return true;
}

void EOS_CALL
EgsOverlay::hide_friends_callback(const EOS_UI_HideFriendsCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_UI_HideFriends failed: {}",
              static_cast<i32>(data->ResultCode));
}

bool EgsOverlay::friends_visible() const {
  if (!m_platform.has_epic_account())
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_GetFriendsVisibleOptions options{};
  options.ApiVersion = EOS_UI_GETFRIENDSVISIBLE_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();

  return EOS_UI_GetFriendsVisible(ui, &options) == EOS_TRUE;
}

bool EgsOverlay::show_block_player(const usize friend_index) const {
  if (!m_platform.has_epic_account())
    return false;
  const EOS_EpicAccountId target = m_presence.friend_id_at(friend_index);
  if (target == nullptr)
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_ShowBlockPlayerOptions options{};
  options.ApiVersion = EOS_UI_SHOWBLOCKPLAYER_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.TargetUserId = target;

  EOS_UI_ShowBlockPlayer(ui, &options, nullptr,
                         &EgsOverlay::show_block_player_callback);
  return true;
}

void EOS_CALL EgsOverlay::show_block_player_callback(
    const EOS_UI_OnShowBlockPlayerCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_UI_ShowBlockPlayer failed: {}",
              static_cast<i32>(data->ResultCode));
}

bool EgsOverlay::show_report_player(const usize friend_index) const {
  if (!m_platform.has_epic_account())
    return false;
  const EOS_EpicAccountId target = m_presence.friend_id_at(friend_index);
  if (target == nullptr)
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_ShowReportPlayerOptions options{};
  options.ApiVersion = EOS_UI_SHOWREPORTPLAYER_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.TargetUserId = target;

  EOS_UI_ShowReportPlayer(ui, &options, nullptr,
                          &EgsOverlay::show_report_player_callback);
  return true;
}

void EOS_CALL EgsOverlay::show_report_player_callback(
    const EOS_UI_OnShowReportPlayerCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_UI_ShowReportPlayer failed: {}",
              static_cast<i32>(data->ResultCode));
}

bool EgsOverlay::show_native_profile(const usize friend_index) const {
  if (!m_platform.has_epic_account())
    return false;
  const EOS_EpicAccountId target = m_presence.friend_id_at(friend_index);
  if (target == nullptr)
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_ShowNativeProfileOptions options{};
  options.ApiVersion = EOS_UI_SHOWNATIVEPROFILE_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.TargetUserId = target;

  EOS_UI_ShowNativeProfile(ui, &options, nullptr,
                           &EgsOverlay::show_native_profile_callback);
  return true;
}

void EOS_CALL EgsOverlay::show_native_profile_callback(
    const EOS_UI_ShowNativeProfileCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_UI_ShowNativeProfile failed: {}",
              static_cast<i32>(data->ResultCode));
}

// Neither call below takes a LocalUserId at all - a process-level UI
// setting, not tied to a signed-in Epic account - so the real precondition
// is just a valid platform handle, unlike every other method above.
bool EgsOverlay::pause_social_overlay(const bool paused) const {
  if (!m_platform.ready())
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_PauseSocialOverlayOptions options{};
  options.ApiVersion = EOS_UI_PAUSESOCIALOVERLAY_API_LATEST;
  options.bIsPaused = paused ? EOS_TRUE : EOS_FALSE;

  return EOS_UI_PauseSocialOverlay(ui, &options) == EOS_EResult::EOS_Success;
}

bool EgsOverlay::social_overlay_paused() const {
  if (!m_platform.ready())
    return false;
  EOS_HUI ui = EOS_Platform_GetUIInterface(m_platform.handle());

  EOS_UI_IsSocialOverlayPausedOptions options{};
  options.ApiVersion = EOS_UI_ISSOCIALOVERLAYPAUSED_API_LATEST;

  return EOS_UI_IsSocialOverlayPaused(ui, &options) == EOS_TRUE;
}

}
