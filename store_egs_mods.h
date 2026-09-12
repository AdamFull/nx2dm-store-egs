#pragma once

#include "store_egs/store_egs_platform.h"

#include "core/foundation/core/foundation.h"
#include "core/foundation/strings/utf8_string.h"

#include <eos_mods.h>

namespace nxm::store_egs {

/// EOS mod management (EOS_HMods) - an EGS-only extra, structurally
/// different from store_steam's Workshop: EOS Mods only manages
/// Epic-launcher-installed mods (install/uninstall/update/enumerate), there
/// is no publish/upload API at all - a real scope difference from Steam
/// Workshop's UGC flow, not an oversight.
///
/// EOS_Mods_InstallMod/UninstallMod/UpdateMod all take a full
/// EOS_Mod_Identifier (namespace/item/artifact id + title + version), not a
/// bare id string, so a mod must first be discovered through
/// EnumerateMods+CopyModInfo - the same index-into-a-cached-list idiom
/// store/store_scripting.cpp's own ProductCache/StringListCache already
/// use, just with a 5-field entry instead of a single string.
///
/// Uses the Epic-account auth group (same as EgsPresence/EgsOverlay), not
/// the device-id one - guards on EgsPlatform::has_epic_account().
struct EgsModEntry {
  nx::string namespace_id;
  nx::string item_id;
  nx::string artifact_id;
  nx::string title;
  nx::string version;
};

class EgsMods {
public:
  explicit EgsMods(EgsPlatform &platform) noexcept : m_platform(platform) {}

  /// Fires EOS_Mods_EnumerateMods(EOS_MET_INSTALLED), refreshing
  /// installed_count()/installed_title()/installed_version().
  void refresh_installed();
  /// Fires EOS_Mods_EnumerateMods(EOS_MET_ALL_AVAILABLE), refreshing
  /// available_count()/available_title()/available_version().
  void refresh_available();

  [[nodiscard]] usize installed_count() const noexcept {
    return m_installed.size();
  }
  [[nodiscard]] nx::string_view installed_title(usize index) const noexcept;
  [[nodiscard]] nx::string_view installed_version(usize index) const noexcept;

  [[nodiscard]] usize available_count() const noexcept {
    return m_available.size();
  }
  [[nodiscard]] nx::string_view available_title(usize index) const noexcept;
  [[nodiscard]] nx::string_view available_version(usize index) const noexcept;

  /// Fires EOS_Mods_InstallMod for available_*(@p available_index)'s cached
  /// identifier.
  bool install(usize available_index);
  [[nodiscard]] bool install_pending() const noexcept {
    return m_install_pending;
  }
  [[nodiscard]] nx::string_view install_error() const {
    return m_install_error.view();
  }

  /// Fires EOS_Mods_UninstallMod for installed_*(@p installed_index)'s
  /// cached identifier.
  bool uninstall(usize installed_index);
  [[nodiscard]] bool uninstall_pending() const noexcept {
    return m_uninstall_pending;
  }
  [[nodiscard]] nx::string_view uninstall_error() const {
    return m_uninstall_error.view();
  }

  /// Fires EOS_Mods_UpdateMod for installed_*(@p installed_index)'s cached
  /// identifier.
  bool update(usize installed_index);
  [[nodiscard]] bool update_pending() const noexcept {
    return m_update_pending;
  }
  [[nodiscard]] nx::string_view update_error() const {
    return m_update_error.view();
  }

private:
  void refresh(EOS_EModEnumerationType type);
  static void EOS_CALL
  enumerate_callback(const EOS_Mods_EnumerateModsCallbackInfo *data);
  void on_enumerate_result(EOS_EResult result, EOS_EModEnumerationType type);

  static void EOS_CALL
  install_callback(const EOS_Mods_InstallModCallbackInfo *data);
  void on_install_result(EOS_EResult result);
  static void EOS_CALL
  uninstall_callback(const EOS_Mods_UninstallModCallbackInfo *data);
  void on_uninstall_result(EOS_EResult result);
  static void EOS_CALL
  update_callback(const EOS_Mods_UpdateModCallbackInfo *data);
  void on_update_result(EOS_EResult result);

  EgsPlatform &m_platform;
  nx::vector<EgsModEntry> m_installed;
  nx::vector<EgsModEntry> m_available;

  bool m_install_pending = false;
  nx::string m_install_error;
  bool m_uninstall_pending = false;
  nx::string m_uninstall_error;
  bool m_update_pending = false;
  nx::string m_update_error;
};

}
