#include "store_egs/store_egs_platform.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_egs {
namespace {

const nx::log::Category log_store_egs = nx::log::category("store_egs");

}

bool EgsPlatform::initialize(const PlatformConfig &config) {
  EOS_InitializeOptions init_options{};
  init_options.ApiVersion = EOS_INITIALIZE_API_LATEST;
  init_options.ProductName = "nx2d";
  init_options.ProductVersion = "1.0";

  const EOS_EResult init_result = EOS_Initialize(&init_options);
  if (init_result != EOS_EResult::EOS_Success &&
      init_result != EOS_EResult::EOS_AlreadyConfigured) {
    nx::logw(log_store_egs, "EOS_Initialize failed: {}",
              static_cast<i32>(init_result));
    return false;
  }
  m_initialized_globally = true;

  EOS_Platform_ClientCredentials credentials{};
  credentials.ClientId = config.client_id.c_str();
  credentials.ClientSecret = config.client_secret.c_str();

  EOS_Platform_Options platform_options{};
  platform_options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
  platform_options.ProductId = config.product_id.c_str();
  platform_options.SandboxId = config.sandbox_id.c_str();
  platform_options.ClientCredentials = credentials;
  platform_options.bIsServer = EOS_FALSE;
  platform_options.EncryptionKey = config.encryption_key.c_str();
  platform_options.DeploymentId = config.deployment_id.c_str();
  // A relative cache directory, mirroring how store_steam writes
  // steam_appid.txt relative to the process's own working directory - EOS
  // requires one for PlayerDataStorage/TitleStorage local caching.
  platform_options.CacheDirectory = "eos_cache";

  m_platform = EOS_Platform_Create(&platform_options);
  if (m_platform == nullptr) {
    nx::logw(log_store_egs, "EOS_Platform_Create failed");
    return false;
  }

  begin_connect_login();

  // Best-effort silent reuse of a previously cached Epic login on this
  // machine, if any - there is no headless way to perform a *first* Epic
  // login (that needs an interactive browser/overlay flow), so this is as
  // close as EGS gets to Steam's "just call SteamAPI_Init()". Failure here
  // (no cached login) is expected, not an error - Ecom/Presence/Friends
  // just stay unavailable, same as a store_steam build with no Steam
  // client running.
  EOS_Auth_Credentials auth_credentials{};
  auth_credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
  auth_credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;

  EOS_Auth_LoginOptions auth_options{};
  auth_options.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
  auth_options.Credentials = &auth_credentials;
  auth_options.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;

  EOS_HAuth auth = EOS_Platform_GetAuthInterface(m_platform);
  EOS_Auth_Login(auth, &auth_options, this, &EgsPlatform::auth_login_callback);

  return true;
}

void EgsPlatform::begin_connect_login() {
  EOS_HConnect connect = EOS_Platform_GetConnectInterface(m_platform);

  EOS_Connect_CreateDeviceIdOptions device_id_options{};
  device_id_options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
  device_id_options.DeviceModel = "nx2d";
  EOS_Connect_CreateDeviceId(connect, &device_id_options, this,
                             &EgsPlatform::create_device_id_callback);
}

void EgsPlatform::on_create_device_id_result(const EOS_EResult result) {
  // EOS_DuplicateNotAllowed means a device id already exists locally from a
  // previous run - expected on every run after the first, not an error.
  if (result != EOS_EResult::EOS_Success &&
      result != EOS_EResult::EOS_DuplicateNotAllowed) {
    nx::logw(log_store_egs, "EOS_Connect_CreateDeviceId failed: {}",
              static_cast<i32>(result));
    return;
  }

  EOS_HConnect connect = EOS_Platform_GetConnectInterface(m_platform);

  EOS_Connect_Credentials credentials{};
  credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
  credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;

  EOS_Connect_UserLoginInfo login_info{};
  login_info.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
  login_info.DisplayName = "Player";

  EOS_Connect_LoginOptions login_options{};
  login_options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
  login_options.Credentials = &credentials;
  login_options.UserLoginInfo = &login_info;

  EOS_Connect_Login(connect, &login_options, this,
                    &EgsPlatform::connect_login_callback);
}

void EgsPlatform::on_connect_login_result(const EOS_EResult result,
                                          const EOS_ProductUserId user,
                                          const EOS_ContinuanceToken continuance) {
  if (result == EOS_EResult::EOS_Success) {
    m_product_user_id = user;
    nx::logi(log_store_egs, "Connect login succeeded");
    return;
  }
  if (result == EOS_EResult::EOS_InvalidUser && continuance != nullptr) {
    EOS_HConnect connect = EOS_Platform_GetConnectInterface(m_platform);
    EOS_Connect_CreateUserOptions create_options{};
    create_options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
    create_options.ContinuanceToken = continuance;
    EOS_Connect_CreateUser(connect, &create_options, this,
                           &EgsPlatform::connect_create_user_callback);
    return;
  }
  nx::logw(log_store_egs, "EOS_Connect_Login failed: {}",
            static_cast<i32>(result));
}

void EgsPlatform::on_connect_create_user_result(const EOS_EResult result,
                                                const EOS_ProductUserId user) {
  if (result == EOS_EResult::EOS_Success) {
    m_product_user_id = user;
    nx::logi(log_store_egs, "Connect account created");
  } else {
    nx::logw(log_store_egs, "EOS_Connect_CreateUser failed: {}",
              static_cast<i32>(result));
  }
}

void EgsPlatform::on_auth_login_result(const EOS_EResult result,
                                       const EOS_EpicAccountId account) {
  if (result == EOS_EResult::EOS_Success) {
    m_epic_account_id = account;
    nx::logi(log_store_egs, "Epic account auth succeeded (cached login reused)");
  } else {
    nx::logi(log_store_egs,
              "no cached Epic login to reuse ({}) - entitlements/presence/"
              "friends stay unavailable this session",
              static_cast<i32>(result));
  }
}

void EOS_CALL EgsPlatform::create_device_id_callback(
    const EOS_Connect_CreateDeviceIdCallbackInfo *const data) {
  static_cast<EgsPlatform *>(data->ClientData)
      ->on_create_device_id_result(data->ResultCode);
}

void EOS_CALL
EgsPlatform::connect_login_callback(const EOS_Connect_LoginCallbackInfo *const data) {
  static_cast<EgsPlatform *>(data->ClientData)
      ->on_connect_login_result(data->ResultCode, data->LocalUserId,
                                data->ContinuanceToken);
}

void EOS_CALL EgsPlatform::connect_create_user_callback(
    const EOS_Connect_CreateUserCallbackInfo *const data) {
  static_cast<EgsPlatform *>(data->ClientData)
      ->on_connect_create_user_result(data->ResultCode, data->LocalUserId);
}

void EOS_CALL
EgsPlatform::auth_login_callback(const EOS_Auth_LoginCallbackInfo *const data) {
  static_cast<EgsPlatform *>(data->ClientData)
      ->on_auth_login_result(data->ResultCode, data->LocalUserId);
}

void EgsPlatform::tick() {
  if (m_platform != nullptr)
    EOS_Platform_Tick(m_platform);
}

bool EgsPlatform::has_product_user() const noexcept {
  return m_product_user_id != nullptr &&
         EOS_ProductUserId_IsValid(m_product_user_id) == EOS_TRUE;
}

bool EgsPlatform::has_epic_account() const noexcept {
  return m_epic_account_id != nullptr &&
         EOS_EpicAccountId_IsValid(m_epic_account_id) == EOS_TRUE;
}

void EgsPlatform::shutdown() {
  if (m_platform != nullptr) {
    EOS_Platform_Release(m_platform);
    m_platform = nullptr;
  }
  if (m_initialized_globally) {
    EOS_Shutdown();
    m_initialized_globally = false;
  }
}

}
