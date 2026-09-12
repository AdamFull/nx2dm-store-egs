#include "store_egs/store_egs_mods.h"

#include "core/foundation/diagnostics/log.h"
#include "core/foundation/strings/format.h"

#include <utility>

namespace nxm::store_egs {
namespace {

const nx::log::Category log_store_egs = nx::log::category("store_egs");

}

void EgsMods::refresh_installed() {
  refresh(EOS_EModEnumerationType::EOS_MET_INSTALLED);
}

void EgsMods::refresh_available() {
  refresh(EOS_EModEnumerationType::EOS_MET_ALL_AVAILABLE);
}

void EgsMods::refresh(const EOS_EModEnumerationType type) {
  if (!m_platform.has_epic_account())
    return;
  EOS_HMods mods = EOS_Platform_GetModsInterface(m_platform.handle());

  EOS_Mods_EnumerateModsOptions options{};
  options.ApiVersion = EOS_MODS_ENUMERATEMODS_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.Type = type;

  EOS_Mods_EnumerateMods(mods, &options, this, &EgsMods::enumerate_callback);
}

void EOS_CALL
EgsMods::enumerate_callback(const EOS_Mods_EnumerateModsCallbackInfo *const data) {
  static_cast<EgsMods *>(data->ClientData)
      ->on_enumerate_result(data->ResultCode, data->Type);
}

void EgsMods::on_enumerate_result(const EOS_EResult result,
                                  const EOS_EModEnumerationType type) {
  if (result != EOS_EResult::EOS_Success) {
    nx::logw(log_store_egs, "EOS_Mods_EnumerateMods failed: {}",
              static_cast<i32>(result));
    return;
  }
  EOS_HMods mods = EOS_Platform_GetModsInterface(m_platform.handle());

  EOS_Mods_CopyModInfoOptions copy_options{};
  copy_options.ApiVersion = EOS_MODS_COPYMODINFO_API_LATEST;
  copy_options.LocalUserId = m_platform.epic_account_id();
  copy_options.Type = type;

  EOS_Mods_ModInfo *info = nullptr;
  if (EOS_Mods_CopyModInfo(mods, &copy_options, &info) !=
          EOS_EResult::EOS_Success ||
      info == nullptr)
    return;

  nx::vector<EgsModEntry> entries;
  entries.reserve(info->ModsCount > 0 ? static_cast<usize>(info->ModsCount) : 0);
  for (int32_t i = 0; i < info->ModsCount; ++i) {
    const EOS_Mod_Identifier &mod = info->Mods[i];
    EgsModEntry entry;
    if (mod.NamespaceId != nullptr)
      entry.namespace_id = mod.NamespaceId;
    if (mod.ItemId != nullptr)
      entry.item_id = mod.ItemId;
    if (mod.ArtifactId != nullptr)
      entry.artifact_id = mod.ArtifactId;
    if (mod.Title != nullptr)
      entry.title = mod.Title;
    if (mod.Version != nullptr)
      entry.version = mod.Version;
    entries.push_back(std::move(entry));
  }
  EOS_Mods_ModInfo_Release(info);

  if (type == EOS_EModEnumerationType::EOS_MET_INSTALLED)
    m_installed = std::move(entries);
  else
    m_available = std::move(entries);
}

nx::string_view EgsMods::installed_title(const usize index) const noexcept {
  return index < m_installed.size() ? m_installed[index].title.view()
                                    : nx::string_view{};
}

nx::string_view EgsMods::installed_version(const usize index) const noexcept {
  return index < m_installed.size() ? m_installed[index].version.view()
                                    : nx::string_view{};
}

nx::string_view EgsMods::available_title(const usize index) const noexcept {
  return index < m_available.size() ? m_available[index].title.view()
                                    : nx::string_view{};
}

nx::string_view EgsMods::available_version(const usize index) const noexcept {
  return index < m_available.size() ? m_available[index].version.view()
                                    : nx::string_view{};
}

namespace {

EOS_Mod_Identifier make_identifier(const EgsModEntry &entry) {
  EOS_Mod_Identifier identifier{};
  identifier.ApiVersion = EOS_MOD_IDENTIFIER_API_LATEST;
  identifier.NamespaceId = entry.namespace_id.c_str();
  identifier.ItemId = entry.item_id.c_str();
  identifier.ArtifactId = entry.artifact_id.c_str();
  identifier.Title = entry.title.c_str();
  identifier.Version = entry.version.c_str();
  return identifier;
}

}

bool EgsMods::install(const usize available_index) {
  if (!m_platform.has_epic_account() || available_index >= m_available.size())
    return false;
  EOS_HMods mods = EOS_Platform_GetModsInterface(m_platform.handle());

  const EOS_Mod_Identifier identifier = make_identifier(m_available[available_index]);
  EOS_Mods_InstallModOptions options{};
  options.ApiVersion = EOS_MODS_INSTALLMOD_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.Mod = &identifier;
  options.bRemoveAfterExit = EOS_FALSE;

  m_install_pending = true;
  m_install_error.clear();
  EOS_Mods_InstallMod(mods, &options, this, &EgsMods::install_callback);
  return true;
}

void EOS_CALL
EgsMods::install_callback(const EOS_Mods_InstallModCallbackInfo *const data) {
  static_cast<EgsMods *>(data->ClientData)->on_install_result(data->ResultCode);
}

void EgsMods::on_install_result(const EOS_EResult result) {
  m_install_pending = false;
  if (result != EOS_EResult::EOS_Success)
    m_install_error = nx::format("EOS error {}", static_cast<i32>(result));
}

bool EgsMods::uninstall(const usize installed_index) {
  if (!m_platform.has_epic_account() || installed_index >= m_installed.size())
    return false;
  EOS_HMods mods = EOS_Platform_GetModsInterface(m_platform.handle());

  const EOS_Mod_Identifier identifier = make_identifier(m_installed[installed_index]);
  EOS_Mods_UninstallModOptions options{};
  options.ApiVersion = EOS_MODS_UNINSTALLMOD_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.Mod = &identifier;

  m_uninstall_pending = true;
  m_uninstall_error.clear();
  EOS_Mods_UninstallMod(mods, &options, this, &EgsMods::uninstall_callback);
  return true;
}

void EOS_CALL
EgsMods::uninstall_callback(const EOS_Mods_UninstallModCallbackInfo *const data) {
  static_cast<EgsMods *>(data->ClientData)->on_uninstall_result(data->ResultCode);
}

void EgsMods::on_uninstall_result(const EOS_EResult result) {
  m_uninstall_pending = false;
  if (result != EOS_EResult::EOS_Success)
    m_uninstall_error = nx::format("EOS error {}", static_cast<i32>(result));
}

bool EgsMods::update(const usize installed_index) {
  if (!m_platform.has_epic_account() || installed_index >= m_installed.size())
    return false;
  EOS_HMods mods = EOS_Platform_GetModsInterface(m_platform.handle());

  const EOS_Mod_Identifier identifier = make_identifier(m_installed[installed_index]);
  EOS_Mods_UpdateModOptions options{};
  options.ApiVersion = EOS_MODS_UPDATEMOD_API_LATEST;
  options.LocalUserId = m_platform.epic_account_id();
  options.Mod = &identifier;

  m_update_pending = true;
  m_update_error.clear();
  EOS_Mods_UpdateMod(mods, &options, this, &EgsMods::update_callback);
  return true;
}

void EOS_CALL
EgsMods::update_callback(const EOS_Mods_UpdateModCallbackInfo *const data) {
  static_cast<EgsMods *>(data->ClientData)->on_update_result(data->ResultCode);
}

void EgsMods::on_update_result(const EOS_EResult result) {
  m_update_pending = false;
  if (result != EOS_EResult::EOS_Success)
    m_update_error = nx::format("EOS error {}", static_cast<i32>(result));
}

}
