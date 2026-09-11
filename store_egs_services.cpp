#include "store_egs/store_egs_services.h"

#include "core/foundation/diagnostics/log.h"

#include <cmath>
#include <cstring>

namespace nxm::store_egs {
namespace {

const nx::log::Category log_store_egs = nx::log::category("store_egs");

}

// -- EgsCore (Ecom entitlements) -----------------------------------------

bool EgsCore::is_owned(const nx::string_view dlc_id) const {
  if (dlc_id.empty())
    // EOS has no single "owns the base app" boolean the way Steam's
    // BIsSubscribed() does - ownership is entitlement-based. A successful
    // Epic account sign-in already implies base-game context in Epic's own
    // model, so it's the closest honest proxy available here.
    return m_platform.has_epic_account();
  for (const nx::string &id : m_entitlement_ids)
    if (id.view() == dlc_id)
      return true;
  return false;
}

nx::vector<nx::string> EgsCore::owned_dlc_ids() const { return m_entitlement_ids; }

void EgsCore::refresh_entitlements() {
  if (!m_platform.has_epic_account() || m_query_pending)
    return;
  EOS_HEcom ecom = EOS_Platform_GetEcomInterface(m_platform.handle());

  EOS_Ecom_QueryEntitlementsOptions options{};
  options.ApiVersion = EOS_ECOM_QUERYENTITLEMENTS_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.bIncludeRedeemed = EOS_TRUE;

  m_query_pending = true;
  EOS_Ecom_QueryEntitlements(ecom, &options, this,
                            &EgsCore::query_entitlements_callback);
}

void EOS_CALL EgsCore::query_entitlements_callback(
    const EOS_Ecom_QueryEntitlementsCallbackInfo *const data) {
  static_cast<EgsCore *>(data->ClientData)
      ->on_query_entitlements_result(data->ResultCode);
}

void EgsCore::on_query_entitlements_result(const EOS_EResult result) {
  m_query_pending = false;
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Ecom_QueryEntitlements failed: {}",
              static_cast<i32>(result));
    return;
  }

  EOS_HEcom ecom = EOS_Platform_GetEcomInterface(m_platform.handle());
  EOS_Ecom_GetEntitlementsCountOptions count_options{};
  count_options.ApiVersion = EOS_ECOM_GETENTITLEMENTSCOUNT_API_LATEST;
  count_options.LocalUserId = m_platform.epic_account_id();
  const uint32_t count = EOS_Ecom_GetEntitlementsCount(ecom, &count_options);

  m_entitlement_ids.clear();
  m_entitlement_ids.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    EOS_Ecom_CopyEntitlementByIndexOptions copy_options{};
    copy_options.ApiVersion = EOS_ECOM_COPYENTITLEMENTBYINDEX_API_LATEST;
    copy_options.LocalUserId = m_platform.epic_account_id();
    copy_options.EntitlementIndex = i;
    EOS_Ecom_Entitlement *entitlement = nullptr;
    if (EOS_Ecom_CopyEntitlementByIndex(ecom, &copy_options, &entitlement) ==
            EOS_EResult::EOS_Success &&
        entitlement != nullptr) {
      if (entitlement->EntitlementName != nullptr)
        m_entitlement_ids.emplace_back(entitlement->EntitlementName);
      EOS_Ecom_Entitlement_Release(entitlement);
    }
  }
}

// -- EgsIap (Ecom offers/checkout) ---------------------------------------

void EgsIap::refresh_offers() {
  if (!m_platform.has_epic_account())
    return;
  EOS_HEcom ecom = EOS_Platform_GetEcomInterface(m_platform.handle());

  EOS_Ecom_QueryOffersOptions options{};
  options.ApiVersion = EOS_ECOM_QUERYOFFERS_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();

  EOS_Ecom_QueryOffers(ecom, &options, this, &EgsIap::query_offers_callback);
}

void EOS_CALL
EgsIap::query_offers_callback(const EOS_Ecom_QueryOffersCallbackInfo *const data) {
  static_cast<EgsIap *>(data->ClientData)->on_query_offers_result(data->ResultCode);
}

void EgsIap::on_query_offers_result(const EOS_EResult result) {
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Ecom_QueryOffers failed: {}",
              static_cast<i32>(result));
    return;
  }

  EOS_HEcom ecom = EOS_Platform_GetEcomInterface(m_platform.handle());
  EOS_Ecom_GetOfferCountOptions count_options{};
  count_options.ApiVersion = EOS_ECOM_GETOFFERCOUNT_API_LATEST;
  count_options.LocalUserId = m_platform.epic_account_id();
  const uint32_t count = EOS_Ecom_GetOfferCount(ecom, &count_options);

  m_products.clear();
  m_products.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    EOS_Ecom_CopyOfferByIndexOptions copy_options{};
    copy_options.ApiVersion = EOS_ECOM_COPYOFFERBYINDEX_API_LATEST;
    copy_options.LocalUserId = m_platform.epic_account_id();
    copy_options.OfferIndex = i;
    EOS_Ecom_CatalogOffer *offer = nullptr;
    if (EOS_Ecom_CopyOfferByIndex(ecom, &copy_options, &offer) !=
            EOS_EResult::EOS_Success ||
        offer == nullptr)
      continue;
    store::StoreProduct product;
    if (offer->Id != nullptr)
      product.id = offer->Id;
    if (offer->TitleText != nullptr)
      product.title = offer->TitleText;
    if (offer->PriceResult == EOS_EResult::EOS_Success && offer->DecimalPoint > 0) {
      const f64 amount =
          static_cast<f64>(offer->CurrentPrice64) /
          std::pow(10.0, static_cast<f64>(offer->DecimalPoint));
      product.price_display = nx::format("{:.2f} {}", amount,
                                         offer->CurrencyCode != nullptr
                                             ? offer->CurrencyCode
                                             : "");
    }
    m_products.push_back(std::move(product));
    EOS_Ecom_CatalogOffer_Release(offer);
  }
}

bool EgsIap::purchase(const nx::string_view product_id) {
  if (!m_platform.has_epic_account())
    return false;
  EOS_HEcom ecom = EOS_Platform_GetEcomInterface(m_platform.handle());

  const nx::string offer_id(product_id);
  EOS_Ecom_CheckoutEntry entry{};
  entry.ApiVersion = EOS_ECOM_CHECKOUTENTRY_API_LATEST;
  entry.OfferId = offer_id.c_str();

  EOS_Ecom_CheckoutOptions options{};
  options.ApiVersion = EOS_ECOM_CHECKOUT_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.EntryCount = 1;
  options.Entries = &entry;

  m_purchase_pending = true;
  m_purchase_error.clear();
  EOS_Ecom_Checkout(ecom, &options, this, &EgsIap::checkout_callback);
  return true;
}

void EOS_CALL
EgsIap::checkout_callback(const EOS_Ecom_CheckoutCallbackInfo *const data) {
  static_cast<EgsIap *>(data->ClientData)->on_checkout_result(data->ResultCode);
}

void EgsIap::on_checkout_result(const EOS_EResult result) {
  m_purchase_pending = false;
  // EOS_Ecom_PurchaseProcessing means the overlay flow finished but the
  // purchase may still be settling server-side - not a hard failure, the
  // same "re-check ownership afterward" shape as a failed check here.
  if (result != EOS_EResult::EOS_Success &&
      result != EOS_EResult::EOS_Ecom_PurchaseProcessing)
    m_purchase_error = nx::format("EOS error {}", static_cast<i32>(result));
}

// -- EgsAchievements (Achievements + Stats) --------------------------

bool EgsAchievements::unlock(const nx::string_view id) {
  if (!m_platform.has_product_user())
    return false;
  EOS_HAchievements achievements =
      EOS_Platform_GetAchievementsInterface(m_platform.handle());

  const nx::string name(id);
  const char *names[] = {name.c_str()};

  EOS_Achievements_UnlockAchievementsOptions options{};
  options.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
  options.UserId = m_platform.product_user_id();
  options.AchievementIds = names;
  options.AchievementsCount = 1;

  EOS_Achievements_UnlockAchievements(achievements, &options, nullptr, nullptr);
  m_unlocked_ids.push_back(name);
  return true;
}

bool EgsAchievements::is_unlocked(const nx::string_view id) const {
  for (const nx::string &unlocked : m_unlocked_ids)
    if (unlocked.view() == id)
      return true;
  return false;
}

nx::vector<nx::string> EgsAchievements::achievement_ids() const {
  return m_achievement_ids;
}

void EgsAchievements::refresh_definitions() {
  if (!m_platform.has_product_user())
    return;
  EOS_HAchievements achievements =
      EOS_Platform_GetAchievementsInterface(m_platform.handle());

  EOS_Achievements_QueryDefinitionsOptions options{};
  options.ApiVersion = EOS_ACHIEVEMENTS_QUERYDEFINITIONS_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();

  EOS_Achievements_QueryDefinitions(achievements, &options, this,
                                    &EgsAchievements::query_definitions_callback);
}

void EOS_CALL EgsAchievements::query_definitions_callback(
    const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo *const data) {
  static_cast<EgsAchievements *>(data->ClientData)
      ->on_query_definitions_result(data->ResultCode);
}

void EgsAchievements::on_query_definitions_result(const EOS_EResult result) {
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Achievements_QueryDefinitions failed: {}",
              static_cast<i32>(result));
    return;
  }
  EOS_HAchievements achievements =
      EOS_Platform_GetAchievementsInterface(m_platform.handle());

  EOS_Achievements_GetAchievementDefinitionCountOptions count_options{};
  count_options.ApiVersion =
      EOS_ACHIEVEMENTS_GETACHIEVEMENTDEFINITIONCOUNT_API_LATEST;
  const uint32_t count =
      EOS_Achievements_GetAchievementDefinitionCount(achievements, &count_options);

  m_achievement_ids.clear();
  m_achievement_ids.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    EOS_Achievements_CopyAchievementDefinitionV2ByIndexOptions copy_options{};
    copy_options.ApiVersion =
        EOS_ACHIEVEMENTS_COPYACHIEVEMENTDEFINITIONV2BYINDEX_API_LATEST;
    copy_options.AchievementIndex = i;
    EOS_Achievements_DefinitionV2 *definition = nullptr;
    if (EOS_Achievements_CopyAchievementDefinitionV2ByIndex(
            achievements, &copy_options, &definition) == EOS_EResult::EOS_Success &&
        definition != nullptr) {
      if (definition->AchievementId != nullptr)
        m_achievement_ids.emplace_back(definition->AchievementId);
      EOS_Achievements_DefinitionV2_Release(definition);
    }
  }
}

void EgsAchievements::refresh_player_achievements() {
  if (!m_platform.has_product_user())
    return;
  EOS_HAchievements achievements =
      EOS_Platform_GetAchievementsInterface(m_platform.handle());

  EOS_Achievements_QueryPlayerAchievementsOptions options{};
  options.ApiVersion = EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST;
  options.TargetUserId = m_platform.product_user_id();
  options.LocalUserId = m_platform.product_user_id();

  EOS_Achievements_QueryPlayerAchievements(
      achievements, &options, this,
      &EgsAchievements::query_player_achievements_callback);
}

void EOS_CALL EgsAchievements::query_player_achievements_callback(
    const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo *const data) {
  static_cast<EgsAchievements *>(data->ClientData)
      ->on_query_player_achievements_result(data->ResultCode);
}

void EgsAchievements::on_query_player_achievements_result(const EOS_EResult result) {
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Achievements_QueryPlayerAchievements failed: {}",
              static_cast<i32>(result));
    return;
  }
  EOS_HAchievements achievements =
      EOS_Platform_GetAchievementsInterface(m_platform.handle());

  EOS_Achievements_GetPlayerAchievementCountOptions count_options{};
  count_options.ApiVersion = EOS_ACHIEVEMENTS_GETPLAYERACHIEVEMENTCOUNT_API_LATEST;
  count_options.UserId = m_platform.product_user_id();
  const uint32_t count =
      EOS_Achievements_GetPlayerAchievementCount(achievements, &count_options);

  m_unlocked_ids.clear();
  for (uint32_t i = 0; i < count; ++i) {
    EOS_Achievements_CopyPlayerAchievementByIndexOptions copy_options{};
    copy_options.ApiVersion = EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST;
    copy_options.TargetUserId = m_platform.product_user_id();
    copy_options.LocalUserId = m_platform.product_user_id();
    copy_options.AchievementIndex = i;
    EOS_Achievements_PlayerAchievement *achievement = nullptr;
    if (EOS_Achievements_CopyPlayerAchievementByIndex(
            achievements, &copy_options, &achievement) == EOS_EResult::EOS_Success &&
        achievement != nullptr) {
      if (achievement->AchievementId != nullptr && achievement->UnlockTime >= 0)
        m_unlocked_ids.emplace_back(achievement->AchievementId);
      EOS_Achievements_PlayerAchievement_Release(achievement);
    }
  }
}

bool EgsAchievements::set_stat(const nx::string_view id, const f64 value) {
  if (!m_platform.has_product_user())
    return false;
  EOS_HStats stats = EOS_Platform_GetStatsInterface(m_platform.handle());

  const nx::string name(id);
  EOS_Stats_IngestData ingest{};
  ingest.ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
  ingest.StatName = name.c_str();
  ingest.IngestAmount = nx::cast<i32>(value);

  EOS_Stats_IngestStatOptions options{};
  options.ApiVersion = EOS_STATS_INGESTSTAT_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();
  options.Stats = &ingest;
  options.StatsCount = 1;

  EOS_Stats_IngestStat(stats, &options, nullptr, nullptr);
  m_stats.insert_or_assign(name, value);
  return true;
}

f64 EgsAchievements::stat(const nx::string_view id) const {
  const auto it = m_stats.find(nx::string(id));
  return it == m_stats.end() ? 0.0 : it->second;
}

void EgsAchievements::refresh_stats() {
  if (!m_platform.has_product_user())
    return;
  EOS_HStats stats = EOS_Platform_GetStatsInterface(m_platform.handle());

  EOS_Stats_QueryStatsOptions options{};
  options.ApiVersion = EOS_STATS_QUERYSTATS_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();
  options.StartTime = -1;
  options.EndTime = -1;

  EOS_Stats_QueryStats(stats, &options, this, &EgsAchievements::query_stats_callback);
}

void EOS_CALL EgsAchievements::query_stats_callback(
    const EOS_Stats_OnQueryStatsCompleteCallbackInfo *const data) {
  static_cast<EgsAchievements *>(data->ClientData)
      ->on_query_stats_result(data->ResultCode);
}

void EgsAchievements::on_query_stats_result(const EOS_EResult result) {
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Stats_QueryStats failed: {}",
              static_cast<i32>(result));
    return;
  }
  EOS_HStats stats = EOS_Platform_GetStatsInterface(m_platform.handle());

  EOS_Stats_GetStatCountOptions count_options{};
  count_options.ApiVersion = EOS_STATS_GETSTATCOUNT_API_LATEST;
  count_options.TargetUserId = m_platform.product_user_id();
  const uint32_t count = EOS_Stats_GetStatsCount(stats, &count_options);

  for (uint32_t i = 0; i < count; ++i) {
    EOS_Stats_CopyStatByIndexOptions copy_options{};
    copy_options.ApiVersion = EOS_STATS_COPYSTATBYINDEX_API_LATEST;
    copy_options.TargetUserId = m_platform.product_user_id();
    copy_options.StatIndex = i;
    EOS_Stats_Stat *stat = nullptr;
    if (EOS_Stats_CopyStatByIndex(stats, &copy_options, &stat) ==
            EOS_EResult::EOS_Success &&
        stat != nullptr) {
      if (stat->Name != nullptr)
        m_stats.insert_or_assign(nx::string(stat->Name),
                                 static_cast<f64>(stat->Value));
      EOS_Stats_Stat_Release(stat);
    }
  }
}

// -- EgsCloudSaves (PlayerDataStorage) --------------------------------

bool EgsCloudSaves::write(const nx::string_view key, const nx::string_view value) {
  if (!m_platform.has_product_user())
    return false;
  EOS_HPlayerDataStorage storage =
      EOS_Platform_GetPlayerDataStorageInterface(m_platform.handle());

  m_write_buffer = nx::string(value);
  m_write_offset = 0;

  const nx::string filename(key);
  EOS_PlayerDataStorage_WriteFileOptions options{};
  options.ApiVersion = EOS_PLAYERDATASTORAGE_WRITEFILE_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();
  options.Filename = filename.c_str();
  options.ChunkLengthBytes = 4096;
  options.WriteFileDataCallback = &EgsCloudSaves::write_data_callback;

  EOS_HPlayerDataStorageFileTransferRequest request = EOS_PlayerDataStorage_WriteFile(
      storage, &options, this, &EgsCloudSaves::write_complete_callback);
  if (request == nullptr)
    return false;
  EOS_PlayerDataStorageFileTransferRequest_Release(request);
  return true;
}

EOS_PlayerDataStorage_EWriteResult EOS_CALL EgsCloudSaves::write_data_callback(
    const EOS_PlayerDataStorage_WriteFileDataCallbackInfo *const data,
    void *const out_data_buffer, uint32_t *const out_data_written) {
  EgsCloudSaves &self = *static_cast<EgsCloudSaves *>(data->ClientData);
  const usize remaining = self.m_write_buffer.size() - self.m_write_offset;
  if (remaining == 0) {
    *out_data_written = 0;
    return EOS_PlayerDataStorage_EWriteResult::EOS_WR_CompleteRequest;
  }
  const usize to_write =
      nx::min(remaining, static_cast<usize>(data->DataBufferLengthBytes));
  std::memcpy(out_data_buffer, self.m_write_buffer.data() + self.m_write_offset,
             to_write);
  self.m_write_offset += to_write;
  *out_data_written = static_cast<uint32_t>(to_write);
  return EOS_PlayerDataStorage_EWriteResult::EOS_WR_ContinueWriting;
}

void EOS_CALL EgsCloudSaves::write_complete_callback(
    const EOS_PlayerDataStorage_WriteFileCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_PlayerDataStorage_WriteFile failed: {}",
              static_cast<i32>(data->ResultCode));
}

nx::string EgsCloudSaves::read(const nx::string_view key) const {
  if (!m_platform.has_product_user())
    return {};
  EOS_HPlayerDataStorage storage =
      EOS_Platform_GetPlayerDataStorageInterface(m_platform.handle());

  m_read_buffer.clear();
  m_pending_read_key = nx::string(key);

  EOS_PlayerDataStorage_ReadFileOptions options{};
  options.ApiVersion = EOS_PLAYERDATASTORAGE_READFILE_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();
  options.Filename = m_pending_read_key.c_str();
  options.ReadChunkLengthBytes = 4096;
  options.ReadFileDataCallback = &EgsCloudSaves::read_data_callback;

  EOS_HPlayerDataStorageFileTransferRequest request = EOS_PlayerDataStorage_ReadFile(
      storage, &options, const_cast<EgsCloudSaves *>(this),
      &EgsCloudSaves::read_complete_callback);
  if (request == nullptr)
    return {};
  EOS_PlayerDataStorageFileTransferRequest_Release(request);
  // The read is asynchronous - like every other query in this module, the
  // first call after a change returns whatever was cached before (empty,
  // the first time), and the real content lands once EgsPlatform::tick()
  // dispatches the completion callback below.
  return m_read_buffer;
}

EOS_PlayerDataStorage_EReadResult EOS_CALL EgsCloudSaves::read_data_callback(
    const EOS_PlayerDataStorage_ReadFileDataCallbackInfo *const data) {
  EgsCloudSaves &self = *static_cast<EgsCloudSaves *>(data->ClientData);
  const auto *const bytes = static_cast<const char *>(data->DataChunk);
  self.m_read_buffer.append(bytes, data->DataChunkLengthBytes);
  return EOS_PlayerDataStorage_EReadResult::EOS_RR_ContinueReading;
}

void EOS_CALL EgsCloudSaves::read_complete_callback(
    const EOS_PlayerDataStorage_ReadFileCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_PlayerDataStorage_ReadFile failed: {}",
              static_cast<i32>(data->ResultCode));
    static_cast<EgsCloudSaves *>(data->ClientData)->m_read_buffer.clear();
  }
}

bool EgsCloudSaves::exists(const nx::string_view key) const {
  if (!m_platform.has_product_user())
    return false;
  EOS_HPlayerDataStorage storage =
      EOS_Platform_GetPlayerDataStorageInterface(m_platform.handle());

  const nx::string filename(key);
  EOS_PlayerDataStorage_CopyFileMetadataByFilenameOptions options{};
  options.ApiVersion = EOS_PLAYERDATASTORAGE_COPYFILEMETADATABYFILENAME_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();
  options.Filename = filename.c_str();

  EOS_PlayerDataStorage_FileMetadata *metadata = nullptr;
  const EOS_EResult result =
      EOS_PlayerDataStorage_CopyFileMetadataByFilename(storage, &options, &metadata);
  if (metadata != nullptr)
    EOS_PlayerDataStorage_FileMetadata_Release(metadata);
  return result == EOS_EResult::EOS_Success;
}

bool EgsCloudSaves::remove(const nx::string_view key) {
  if (!m_platform.has_product_user())
    return false;
  EOS_HPlayerDataStorage storage =
      EOS_Platform_GetPlayerDataStorageInterface(m_platform.handle());

  const nx::string filename(key);
  EOS_PlayerDataStorage_DeleteFileOptions options{};
  options.ApiVersion = EOS_PLAYERDATASTORAGE_DELETEFILE_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();
  options.Filename = filename.c_str();

  EOS_PlayerDataStorage_DeleteFile(storage, &options, nullptr,
                                   &EgsCloudSaves::delete_file_callback);
  return true;
}

void EOS_CALL EgsCloudSaves::delete_file_callback(
    const EOS_PlayerDataStorage_DeleteFileCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_PlayerDataStorage_DeleteFile failed: {}",
              static_cast<i32>(data->ResultCode));
}

nx::vector<nx::string> EgsCloudSaves::keys() const { return m_keys; }

void EgsCloudSaves::refresh_keys() {
  if (!m_platform.has_product_user())
    return;
  EOS_HPlayerDataStorage storage =
      EOS_Platform_GetPlayerDataStorageInterface(m_platform.handle());

  EOS_PlayerDataStorage_QueryFileListOptions options{};
  options.ApiVersion = EOS_PLAYERDATASTORAGE_QUERYFILELIST_API_LATEST;
  options.LocalUserId = m_platform.product_user_id();

  EOS_PlayerDataStorage_QueryFileList(storage, &options, this,
                                      &EgsCloudSaves::query_file_list_callback);
}

void EOS_CALL EgsCloudSaves::query_file_list_callback(
    const EOS_PlayerDataStorage_QueryFileListCallbackInfo *const data) {
  auto &self = *static_cast<EgsCloudSaves *>(data->ClientData);
  if (data->ResultCode != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_PlayerDataStorage_QueryFileList failed: {}",
              static_cast<i32>(data->ResultCode));
    return;
  }

  EOS_HPlayerDataStorage storage =
      EOS_Platform_GetPlayerDataStorageInterface(self.m_platform.handle());
  EOS_PlayerDataStorage_GetFileMetadataCountOptions count_options{};
  count_options.ApiVersion = EOS_PLAYERDATASTORAGE_GETFILEMETADATACOUNT_API_LATEST;
  count_options.LocalUserId = self.m_platform.product_user_id();
  int32_t count = 0;
  EOS_PlayerDataStorage_GetFileMetadataCount(storage, &count_options, &count);

  self.m_keys.clear();
  self.m_keys.reserve(count > 0 ? static_cast<usize>(count) : 0);
  for (int32_t i = 0; i < count; ++i) {
    EOS_PlayerDataStorage_CopyFileMetadataAtIndexOptions copy_options{};
    copy_options.ApiVersion = EOS_PLAYERDATASTORAGE_COPYFILEMETADATAATINDEX_API_LATEST;
    copy_options.LocalUserId = self.m_platform.product_user_id();
    copy_options.Index = static_cast<uint32_t>(i);
    EOS_PlayerDataStorage_FileMetadata *metadata = nullptr;
    if (EOS_PlayerDataStorage_CopyFileMetadataAtIndex(storage, &copy_options,
                                                      &metadata) ==
            EOS_EResult::EOS_Success &&
        metadata != nullptr) {
      if (metadata->Filename != nullptr)
        self.m_keys.emplace_back(metadata->Filename);
      EOS_PlayerDataStorage_FileMetadata_Release(metadata);
    }
  }
}

// -- EgsPresence (Presence + Friends + UserInfo) -----------------------

bool EgsPresence::set_status(const nx::string_view text) {
  if (!m_platform.has_epic_account())
    return false;
  EOS_HPresence presence = EOS_Platform_GetPresenceInterface(m_platform.handle());

  EOS_Presence_CreatePresenceModificationOptions create_options{};
  create_options.ApiVersion = EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST;
  create_options.LocalUserId = m_platform.epic_account_id();
  EOS_HPresenceModification modification = nullptr;
  if (EOS_Presence_CreatePresenceModification(presence, &create_options,
                                              &modification) !=
          EOS_EResult::EOS_Success ||
      modification == nullptr)
    return false;

  const nx::string rich_text(text);
  EOS_PresenceModification_SetRawRichTextOptions rich_text_options{};
  rich_text_options.ApiVersion = EOS_PRESENCEMODIFICATION_SETRAWRICHTEXT_API_LATEST;
  rich_text_options.RichText = rich_text.c_str();
  EOS_PresenceModification_SetRawRichText(modification, &rich_text_options);

  EOS_PresenceModification_SetStatusOptions status_options{};
  status_options.ApiVersion = EOS_PRESENCEMODIFICATION_SETSTATUS_API_LATEST;
  status_options.Status = EOS_Presence_EStatus::EOS_PS_Online;
  EOS_PresenceModification_SetStatus(modification, &status_options);

  EOS_Presence_SetPresenceOptions set_options{};
  set_options.ApiVersion = EOS_PRESENCE_SETPRESENCE_API_LATEST;
  set_options.LocalUserId = m_platform.epic_account_id();
  set_options.PresenceModificationHandle = modification;
  EOS_Presence_SetPresence(presence, &set_options, nullptr,
                           &EgsPresence::set_presence_callback);

  EOS_PresenceModification_Release(modification);
  return true;
}

void EOS_CALL EgsPresence::set_presence_callback(
    const EOS_Presence_SetPresenceCallbackInfo *const data) {
  if (data->ResultCode != EOS_EResult::EOS_Success)
    nx::logw(log_store_egs, "EOS_Presence_SetPresence failed: {}",
              static_cast<i32>(data->ResultCode));
}

void EgsPresence::refresh_own_name() {
  if (!m_platform.has_epic_account())
    return;
  EOS_HUserInfo user_info = EOS_Platform_GetUserInfoInterface(m_platform.handle());

  EOS_UserInfo_QueryUserInfoOptions options{};
  options.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.TargetUserId = m_platform.epic_account_id();

  EOS_UserInfo_QueryUserInfo(user_info, &options, this,
                            &EgsPresence::query_own_user_info_callback);
}

void EOS_CALL EgsPresence::query_own_user_info_callback(
    const EOS_UserInfo_QueryUserInfoCallbackInfo *const data) {
  auto &self = *static_cast<EgsPresence *>(data->ClientData);
  if (data->ResultCode != EOS_EResult::EOS_Success)
    return;

  EOS_HUserInfo user_info = EOS_Platform_GetUserInfoInterface(self.m_platform.handle());
  EOS_UserInfo_CopyUserInfoOptions copy_options{};
  copy_options.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
  copy_options.LocalUserId = data->LocalUserId;
  copy_options.TargetUserId = data->TargetUserId;
  EOS_UserInfo *info = nullptr;
  if (EOS_UserInfo_CopyUserInfo(user_info, &copy_options, &info) ==
          EOS_EResult::EOS_Success &&
      info != nullptr) {
    if (info->DisplayName != nullptr)
      self.m_own_name = info->DisplayName;
    EOS_UserInfo_Release(info);
  }
}

void EgsPresence::refresh_friends() {
  if (!m_platform.has_epic_account())
    return;
  EOS_HFriends friends = EOS_Platform_GetFriendsInterface(m_platform.handle());

  EOS_Friends_QueryFriendsOptions options{};
  options.ApiVersion = EOS_FRIENDS_QUERYFRIENDS_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();

  EOS_Friends_QueryFriends(friends, &options, this,
                           &EgsPresence::query_friends_callback);
}

void EOS_CALL EgsPresence::query_friends_callback(
    const EOS_Friends_QueryFriendsCallbackInfo *const data) {
  static_cast<EgsPresence *>(data->ClientData)
      ->on_query_friends_result(data->ResultCode);
}

void EgsPresence::on_query_friends_result(const EOS_EResult result) {
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Friends_QueryFriends failed: {}",
              static_cast<i32>(result));
    return;
  }
  EOS_HFriends friends = EOS_Platform_GetFriendsInterface(m_platform.handle());
  EOS_HUserInfo user_info = EOS_Platform_GetUserInfoInterface(m_platform.handle());

  EOS_Friends_GetFriendsCountOptions count_options{};
  count_options.ApiVersion = EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST;
  count_options.LocalUserId = m_platform.epic_account_id();
  const int32_t count = EOS_Friends_GetFriendsCount(friends, &count_options);

  m_friend_ids.clear();
  m_friend_names.clear();
  m_friend_ids.reserve(count > 0 ? static_cast<usize>(count) : 0);
  for (int32_t i = 0; i < count; ++i) {
    EOS_Friends_GetFriendAtIndexOptions index_options{};
    index_options.ApiVersion = EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST;
    index_options.LocalUserId = m_platform.epic_account_id();
    index_options.Index = i;
    const EOS_EpicAccountId friend_id = EOS_Friends_GetFriendAtIndex(friends, &index_options);
    if (friend_id == nullptr)
      continue;
    m_friend_ids.push_back(friend_id);

    // Query and immediately try a cached read - the name only resolves once
    // this per-friend query lands (another async round trip), same
    // eventually-consistent shape as everything else in this module; a
    // repeated call to friend_names() picks up names as they arrive.
    EOS_UserInfo_QueryUserInfoOptions query_options{};
    query_options.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
    query_options.LocalUserId = m_platform.epic_account_id();
    query_options.TargetUserId = friend_id;
    EOS_UserInfo_QueryUserInfo(user_info, &query_options, this,
                              &EgsPresence::query_user_info_callback);

    EOS_UserInfo_CopyUserInfoOptions copy_options{};
    copy_options.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
    copy_options.LocalUserId = m_platform.epic_account_id();
    copy_options.TargetUserId = friend_id;
    EOS_UserInfo *info = nullptr;
    if (EOS_UserInfo_CopyUserInfo(user_info, &copy_options, &info) ==
            EOS_EResult::EOS_Success &&
        info != nullptr) {
      m_friend_names.emplace_back(info->DisplayName != nullptr ? info->DisplayName
                                                                : "");
      EOS_UserInfo_Release(info);
    } else {
      m_friend_names.emplace_back();
    }
  }
}

void EOS_CALL EgsPresence::query_user_info_callback(
    const EOS_UserInfo_QueryUserInfoCallbackInfo *const data) {
  auto &self = *static_cast<EgsPresence *>(data->ClientData);
  if (data->ResultCode != EOS_EResult::EOS_Success)
    return;
  EOS_HUserInfo user_info = EOS_Platform_GetUserInfoInterface(self.m_platform.handle());
  EOS_UserInfo_CopyUserInfoOptions copy_options{};
  copy_options.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
  copy_options.LocalUserId = data->LocalUserId;
  copy_options.TargetUserId = data->TargetUserId;
  EOS_UserInfo *info = nullptr;
  if (EOS_UserInfo_CopyUserInfo(user_info, &copy_options, &info) !=
          EOS_EResult::EOS_Success ||
      info == nullptr)
    return;
  for (usize i = 0; i < self.m_friend_ids.size(); ++i)
    if (self.m_friend_ids[i] == data->TargetUserId)
      self.m_friend_names[i] = info->DisplayName != nullptr ? info->DisplayName : "";
  EOS_UserInfo_Release(info);
}

nx::vector<nx::string> EgsPresence::friend_names() const { return m_friend_names; }

}
