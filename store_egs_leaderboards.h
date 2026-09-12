#pragma once

#include "store_egs/store_egs_platform.h"

#include "core/foundation/core/foundation.h"
#include "core/foundation/strings/utf8_string.h"

#include <eos_leaderboards.h>

namespace nxm::store_egs {

/// EOS leaderboards - an EGS-only extra. Simpler than store_steam's
/// SteamLeaderboards: EOS_Leaderboards_QueryLeaderboardRanks takes a
/// LeaderboardId directly (no separate "find" step first), and
/// EOS_Leaderboards_LeaderboardRecord already carries rank/score/display
/// name together (no per-entry friends lookup needed either). There is no
/// score-upload API here at all - a leaderboard is tied to a stat name
/// server-side, so submitting a score is just calling the existing
/// store_set_stat() (EOS_Stats_IngestStat, see EgsAchievements::set_stat).
///
/// Uses the same EOS_ProductUserId (device-id, headless) auth group as
/// achievements/stats, not the Epic-account one - guards on
/// EgsPlatform::has_product_user(), same as EgsAchievements does.
class EgsLeaderboards {
public:
  explicit EgsLeaderboards(EgsPlatform &platform) noexcept
      : m_platform(platform) {}

  /// Fires EOS_Leaderboards_QueryLeaderboardRanks, refreshing entry_count()/
  /// entry_rank()/entry_score()/entry_name().
  bool download(nx::string_view leaderboard_id);
  [[nodiscard]] bool download_pending() const noexcept {
    return m_download_pending;
  }
  [[nodiscard]] usize entry_count() const noexcept { return m_entries.size(); }
  [[nodiscard]] u32 entry_rank(usize index) const noexcept;
  [[nodiscard]] i32 entry_score(usize index) const noexcept;
  [[nodiscard]] nx::string_view entry_name(usize index) const noexcept;

private:
  static void EOS_CALL query_ranks_callback(
      const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo *data);
  void on_query_ranks_result(EOS_EResult result);

  EgsPlatform &m_platform;
  bool m_download_pending = false;

  struct Entry {
    u32 rank = 0;
    i32 score = 0;
    nx::string name;
  };
  nx::vector<Entry> m_entries;
};

}
