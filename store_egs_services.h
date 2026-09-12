#pragma once

#include "store_egs/store_egs_platform.h"

#include "store/store_service.h"

#include "core/foundation/containers/hash_map.h"

#include <eos_achievements.h>
#include <eos_ecom.h>
#include <eos_friends.h>
#include <eos_playerdatastorage.h>
#include <eos_presence.h>
#include <eos_stats.h>
#include <eos_userinfo.h>

namespace nxm::store_egs {

/// The five neutral services (store_service.h), backed by the real EOS SDK.
/// Every EOS call is asynchronous (a plain C completion callback invoked
/// from EgsPlatform::tick() -> EOS_Platform_Tick, the same role
/// store_steam's SteamAPI_RunCallbacks() pump plays) - unlike Steam, even a
/// basic read like "is this achievement unlocked" has no synchronous local
/// cache to query directly. Every class here therefore holds a small cache
/// populated by its own query's completion callback; the neutral
/// interface's synchronous methods read that cache, returning a safe
/// default (false/empty/0) until the first query lands - the same
/// eventually-consistent shape a real game would have to handle itself
/// against this SDK regardless.
///
/// Ecom (entitlements/IAP) and Presence/Friends/UserInfo all require an
/// EOS_EpicAccountId (a full Epic account login) - see store_egs_platform.h
/// for why that's a best-effort, often-unavailable thing here. Each class
/// below checks EgsPlatform::has_epic_account()/has_product_user() first
/// and degrades to the same safe defaults when it's absent, exactly like
/// Stove's SDK lacking cloud saves/presence degrades the neutral surface
/// elsewhere in this module family.

class EgsCore final : public store::StoreCore {
public:
  explicit EgsCore(EgsPlatform &platform) noexcept : m_platform(platform) {}

  [[nodiscard]] bool is_owned(nx::string_view dlc_id = {}) const override;
  [[nodiscard]] nx::vector<nx::string> owned_dlc_ids() const override;
  [[nodiscard]] nx::string_view store_name() const noexcept override {
    return "egs";
  }

  /// Fires EOS_Ecom_QueryEntitlements(EntitlementNameCount=0) - queries
  /// every entitlement the signed-in Epic account has, refreshing the
  /// cache is_owned()/owned_dlc_ids() read from.
  void refresh_entitlements();

  /// EOS always queries every entitlement in one round trip - @p dlc_id is
  /// ignored, same as store::StoreCore::refresh_ownership() documents for
  /// any bulk-capable backend.
  void refresh_ownership(nx::string_view = {}) override {
    refresh_entitlements();
  }

private:
  static void EOS_CALL
  query_entitlements_callback(const EOS_Ecom_QueryEntitlementsCallbackInfo *data);
  void on_query_entitlements_result(EOS_EResult result);

  EgsPlatform &m_platform;
  nx::vector<nx::string> m_entitlement_ids;
  bool m_query_pending = false;
};

/// EOS's IAP flow (EOS_Ecom_Checkout) opens the same kind of native overlay
/// purchase UI Steam's overlay-to-store does - the checkout callback tells
/// you the overlay flow finished, not that the purchase settled server-side
/// (EOS_Ecom_PurchaseProcessing means "keep polling entitlements for a
/// while"), so purchase_pending()/purchase_error() report the checkout
/// call's own state, not a guaranteed final settlement - a game should
/// re-check EgsCore::is_owned() afterward, the same advice store_steam's
/// SteamIap gives.
class EgsIap final : public store::StoreIap {
public:
  explicit EgsIap(EgsPlatform &platform) noexcept : m_platform(platform) {}

  [[nodiscard]] nx::vector<store::StoreProduct> products() const override {
    return m_products;
  }
  bool purchase(nx::string_view product_id) override;
  [[nodiscard]] bool purchase_pending() const override { return m_purchase_pending; }
  [[nodiscard]] nx::string_view purchase_error() const override {
    return m_purchase_error.view();
  }

  /// Fires EOS_Ecom_QueryOffers, refreshing the cache products() reads from.
  void refresh_offers();

  /// EOS always queries every offer in one round trip - @p product_ids is
  /// ignored, same as store::StoreIap::refresh_products() documents for any
  /// bulk-capable backend.
  void refresh_products(const nx::vector<nx::string> & = {}) override {
    refresh_offers();
  }

private:
  static void EOS_CALL
  query_offers_callback(const EOS_Ecom_QueryOffersCallbackInfo *data);
  static void EOS_CALL checkout_callback(const EOS_Ecom_CheckoutCallbackInfo *data);
  void on_query_offers_result(EOS_EResult result);
  void on_checkout_result(EOS_EResult result);

  EgsPlatform &m_platform;
  nx::vector<store::StoreProduct> m_products;
  bool m_purchase_pending = false;
  nx::string m_purchase_error;
};

class EgsAchievements final : public store::StoreAchievements {
public:
  explicit EgsAchievements(EgsPlatform &platform) noexcept : m_platform(platform) {}

  bool unlock(nx::string_view id) override;
  [[nodiscard]] bool is_unlocked(nx::string_view id) const override;
  [[nodiscard]] nx::vector<nx::string> achievement_ids() const override;
  bool set_stat(nx::string_view id, f64 value) override;
  [[nodiscard]] f64 stat(nx::string_view id) const override;

  /// Fires EOS_Achievements_QueryDefinitions, refreshing achievement_ids().
  void refresh_definitions();
  /// Fires EOS_Achievements_QueryPlayerAchievements, refreshing
  /// is_unlocked().
  void refresh_player_achievements();
  /// Fires EOS_Stats_QueryStats (all stats, one round trip), refreshing
  /// every cached value stat() reads from.
  void refresh_stats();

  /// EOS already queries every achievement definition/unlock/stat in bulk -
  /// @p stat_ids is ignored, same as store::StoreAchievements::refresh()
  /// documents for any bulk-capable backend.
  void refresh(const nx::vector<nx::string> & = {}) override {
    refresh_definitions();
    refresh_player_achievements();
    refresh_stats();
  }

private:
  static void EOS_CALL query_definitions_callback(
      const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo *data);
  static void EOS_CALL query_player_achievements_callback(
      const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo *data);
  static void EOS_CALL
  query_stats_callback(const EOS_Stats_OnQueryStatsCompleteCallbackInfo *data);
  void on_query_definitions_result(EOS_EResult result);
  void on_query_player_achievements_result(EOS_EResult result);
  void on_query_stats_result(EOS_EResult result);

  EgsPlatform &m_platform;
  nx::vector<nx::string> m_achievement_ids;
  nx::vector<nx::string> m_unlocked_ids;
  nx::hash_map<nx::string, f64> m_stats;
};

class EgsCloudSaves final : public store::StoreCloudSaves {
public:
  explicit EgsCloudSaves(EgsPlatform &platform) noexcept : m_platform(platform) {}

  bool write(nx::string_view key, nx::string_view value) override;
  [[nodiscard]] nx::string read(nx::string_view key) const override;
  [[nodiscard]] bool exists(nx::string_view key) const override;
  bool remove(nx::string_view key) override;
  [[nodiscard]] nx::vector<nx::string> keys() const override;
  /// EOS's PlayerDataStorage has no quota-query API at all (confirmed
  /// absent from the SDK) - a write past the limit fails with its own
  /// result code instead, so these two always report 0.
  [[nodiscard]] u64 bytes_used() const override { return 0; }
  [[nodiscard]] u64 bytes_total() const override { return 0; }

  /// Fires EOS_PlayerDataStorage_QueryFileList, refreshing keys().
  void refresh_keys() override;

private:
  static EOS_PlayerDataStorage_EReadResult EOS_CALL
  read_data_callback(const EOS_PlayerDataStorage_ReadFileDataCallbackInfo *data);
  static void EOS_CALL
  read_complete_callback(const EOS_PlayerDataStorage_ReadFileCallbackInfo *data);
  static EOS_PlayerDataStorage_EWriteResult EOS_CALL write_data_callback(
      const EOS_PlayerDataStorage_WriteFileDataCallbackInfo *data,
      void *out_data_buffer, uint32_t *out_data_written);
  static void EOS_CALL
  write_complete_callback(const EOS_PlayerDataStorage_WriteFileCallbackInfo *data);
  static void EOS_CALL
  delete_file_callback(const EOS_PlayerDataStorage_DeleteFileCallbackInfo *data);
  static void EOS_CALL
  query_file_list_callback(const EOS_PlayerDataStorage_QueryFileListCallbackInfo *data);

  EgsPlatform &m_platform;
  // Read/write both stage their bytes here across the streaming callbacks -
  // one in-flight transfer of each kind at a time, same simplification
  // store_steam's async ops already use.
  mutable nx::string m_read_buffer;
  mutable nx::string m_pending_read_key;
  nx::string m_write_buffer;
  usize m_write_offset = 0;
  nx::vector<nx::string> m_keys;
};

class EgsPresence final : public store::StorePresence {
public:
  explicit EgsPresence(EgsPlatform &platform) noexcept : m_platform(platform) {}

  bool set_status(nx::string_view text) override;
  [[nodiscard]] nx::string_view own_name() const override { return m_own_name.view(); }
  [[nodiscard]] usize friend_count() const override { return m_friend_ids.size(); }
  [[nodiscard]] nx::vector<nx::string> friend_names() const override;

  /// The raw EOS_EpicAccountId behind friend_names()'s index @p index, or
  /// nullptr if out of range - store_egs_overlay.h's block/report/
  /// native-profile calls need this (EOS_UI's TargetUserId), the same
  /// index-not-raw-handle shape store_steam's own open_to_friend() already
  /// uses for the same reason (Luau has no safe way to carry an opaque
  /// account handle).
  [[nodiscard]] EOS_EpicAccountId friend_id_at(usize index) const noexcept {
    return index < m_friend_ids.size() ? m_friend_ids[index] : nullptr;
  }

  /// Fires EOS_UserInfo_QueryUserInfo for the local user, refreshing
  /// own_name().
  void refresh_own_name();
  /// Fires EOS_Friends_QueryFriends, refreshing friend_count()/
  /// friend_names() (the latter needs a further per-friend UserInfo query,
  /// done inline as each friend id comes back).
  void refresh_friends();

  void refresh() override {
    refresh_own_name();
    refresh_friends();
  }

private:
  static void EOS_CALL
  set_presence_callback(const EOS_Presence_SetPresenceCallbackInfo *data);
  static void EOS_CALL
  query_friends_callback(const EOS_Friends_QueryFriendsCallbackInfo *data);
  static void EOS_CALL
  query_user_info_callback(const EOS_UserInfo_QueryUserInfoCallbackInfo *data);
  static void EOS_CALL query_own_user_info_callback(
      const EOS_UserInfo_QueryUserInfoCallbackInfo *data);
  void on_query_friends_result(EOS_EResult result);

  EgsPlatform &m_platform;
  nx::string m_own_name;
  nx::vector<EOS_EpicAccountId> m_friend_ids;
  nx::vector<nx::string> m_friend_names;
};

}
