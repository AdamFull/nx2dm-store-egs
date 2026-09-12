#include "store_egs/store_egs_leaderboards.h"

#include "core/foundation/diagnostics/log.h"

#include <utility>

namespace nxm::store_egs {
namespace {

const nx::log::Category log_store_egs = nx::log::category("store_egs");

}

bool EgsLeaderboards::download(const nx::string_view leaderboard_id) {
  if (!m_platform.has_product_user())
    return false;
  EOS_HLeaderboards leaderboards =
      EOS_Platform_GetLeaderboardsInterface(m_platform.handle());

  const nx::string id(leaderboard_id);
  EOS_Leaderboards_QueryLeaderboardRanksOptions options{};
  options.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST;
  options.LeaderboardId = id.c_str();
  options.LocalUserId = m_platform.product_user_id();

  m_download_pending = true;
  EOS_Leaderboards_QueryLeaderboardRanks(
      leaderboards, &options, this, &EgsLeaderboards::query_ranks_callback);
  return true;
}

void EOS_CALL EgsLeaderboards::query_ranks_callback(
    const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo *const
        data) {
  static_cast<EgsLeaderboards *>(data->ClientData)
      ->on_query_ranks_result(data->ResultCode);
}

void EgsLeaderboards::on_query_ranks_result(const EOS_EResult result) {
  m_download_pending = false;
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Leaderboards_QueryLeaderboardRanks failed: {}",
              static_cast<i32>(result));
    return;
  }

  EOS_HLeaderboards leaderboards =
      EOS_Platform_GetLeaderboardsInterface(m_platform.handle());
  EOS_Leaderboards_GetLeaderboardRecordCountOptions count_options{};
  count_options.ApiVersion =
      EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST;
  const uint32_t count =
      EOS_Leaderboards_GetLeaderboardRecordCount(leaderboards, &count_options);

  m_entries.clear();
  m_entries.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions copy_options{};
    copy_options.ApiVersion =
        EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST;
    copy_options.LeaderboardRecordIndex = i;
    EOS_Leaderboards_LeaderboardRecord *record = nullptr;
    if (EOS_Leaderboards_CopyLeaderboardRecordByIndex(
            leaderboards, &copy_options, &record) != EOS_EResult::EOS_Success ||
        record == nullptr)
      continue;
    Entry entry;
    entry.rank = record->Rank;
    entry.score = record->Score;
    if (record->UserDisplayName != nullptr)
      entry.name = record->UserDisplayName;
    m_entries.push_back(std::move(entry));
    EOS_Leaderboards_LeaderboardRecord_Release(record);
  }
}

u32 EgsLeaderboards::entry_rank(const usize index) const noexcept {
  return index < m_entries.size() ? m_entries[index].rank : 0;
}

i32 EgsLeaderboards::entry_score(const usize index) const noexcept {
  return index < m_entries.size() ? m_entries[index].score : 0;
}

nx::string_view EgsLeaderboards::entry_name(const usize index) const noexcept {
  return index < m_entries.size() ? m_entries[index].name.view()
                                   : nx::string_view{};
}

}
