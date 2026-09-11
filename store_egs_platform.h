#pragma once

#include "core/foundation/strings/utf8_string.h"

#include <eos_auth.h>
#include <eos_sdk.h>

namespace nxm::store_egs {

struct PlatformConfig {
  nx::string product_id;
  nx::string sandbox_id;
  nx::string deployment_id;
  nx::string client_id;
  nx::string client_secret;
  nx::string encryption_key;
};

/// Owns the EOS_HPlatform lifecycle and the two auth flows EOS splits
/// entitlements/social from everything else with (see store_egs_services.h
/// for which neutral service needs which):
///
/// - EOS_Connect_Login with a device id: silent, headless, no real Epic
///   account needed. Gives an EOS_ProductUserId, which Achievements, Stats,
///   PlayerDataStorage (cloud saves) and Leaderboards all key off. Always
///   attempted.
/// - EOS_Auth_Login with EOS_LCT_PersistentAuth: silently reuses a
///   previously cached Epic login on this machine, if any - there is no
///   headless way to perform a *first* Epic login (EOS_LCT_AccountPortal is
///   an interactive browser/overlay flow), so this is the closest EGS
///   equivalent of Steam's "just call SteamAPI_Init()". Gives an
///   EOS_EpicAccountId, which Ecom (entitlements/IAP), Presence, Friends and
///   UserInfo all key off. Fails silently when nothing is cached - the same
///   honest-failure shape store_steam's SteamAPI_Init() already has when no
///   Steam client is running.
class EgsPlatform {
public:
  bool initialize(const PlatformConfig &config);
  void tick();
  void shutdown();

  [[nodiscard]] bool ready() const noexcept { return m_platform != nullptr; }
  [[nodiscard]] EOS_HPlatform handle() const noexcept { return m_platform; }

  [[nodiscard]] EOS_ProductUserId product_user_id() const noexcept {
    return m_product_user_id;
  }
  [[nodiscard]] bool has_product_user() const noexcept;

  [[nodiscard]] EOS_EpicAccountId epic_account_id() const noexcept {
    return m_epic_account_id;
  }
  [[nodiscard]] bool has_epic_account() const noexcept;

private:
  void begin_connect_login();
  void on_create_device_id_result(EOS_EResult result);
  void on_connect_login_result(EOS_EResult result, EOS_ProductUserId user,
                               EOS_ContinuanceToken continuance);
  void on_connect_create_user_result(EOS_EResult result, EOS_ProductUserId user);
  void on_auth_login_result(EOS_EResult result, EOS_EpicAccountId account);

  static void EOS_CALL
  create_device_id_callback(const EOS_Connect_CreateDeviceIdCallbackInfo *data);
  static void EOS_CALL connect_login_callback(const EOS_Connect_LoginCallbackInfo *data);
  static void EOS_CALL
  connect_create_user_callback(const EOS_Connect_CreateUserCallbackInfo *data);
  static void EOS_CALL auth_login_callback(const EOS_Auth_LoginCallbackInfo *data);

  EOS_HPlatform m_platform = nullptr;
  EOS_ProductUserId m_product_user_id = nullptr;
  EOS_EpicAccountId m_epic_account_id = nullptr;
  bool m_initialized_globally = false;
};

}
