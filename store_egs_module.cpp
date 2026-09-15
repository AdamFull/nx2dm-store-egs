#include "store_egs/store_egs_config.h"
#include "store_egs/store_egs_leaderboards.h"
#include "store_egs/store_egs_mods.h"
#include "store_egs/store_egs_overlay.h"
#include "store_egs/store_egs_platform.h"
#include "store_egs/store_egs_scripting.h"
#include "store_egs/store_egs_services.h"

#include "store/store_service.h"

#include "app/engine.h"
#include "app/module_system/module.h"
#include "app/module_system/module_context.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_egs {
namespace {

constexpr nx::string_view PUMP_SYSTEM = "store_egs.pump";
const nx::log::Category log_store_egs = nx::log::category("store_egs");

constexpr nxe::ModuleService PROVIDED_SERVICES[] = {
    {.id = store::kCoreService, .version = {1, 0, 0}},
    {.id = store::kIapService, .version = {1, 0, 0}},
    {.id = store::kAchievementsService, .version = {1, 0, 0}},
    {.id = store::kCloudSavesService, .version = {1, 0, 0}},
    {.id = store::kPresenceService, .version = {1, 0, 0}},
};

class StoreEgsModule final : public nxe::Module {
public:
  StoreEgsModule()
      : m_core(m_platform), m_iap(m_platform), m_achievements(m_platform),
        m_cloud_saves(m_platform), m_presence(m_platform),
        m_leaderboards(m_platform), m_overlay(m_platform, m_presence),
        m_mods(m_platform) {}

  [[nodiscard]] nxe::ModuleDescriptor descriptor() const noexcept override {
    nxe::ModuleDescriptor out{};
    out.id = "store_egs";
    out.version = {1, 0, 0};
    out.provided_services = PROVIDED_SERVICES;
    // The EOS SDK ships Windows/Linux/macOS - no mobile, no console.
    out.platforms = nxe::ModulePlatform::Windows | nxe::ModulePlatform::Linux |
                    nxe::ModulePlatform::MacOS;
    return out;
  }

  bool on_register(nxe::ModuleContext &ctx) override {
    nxe::ServiceRegistrar registrar = ctx.service_registrar();
    store::StoreCore &core = m_core;
    store::StoreIap &iap = m_iap;
    store::StoreAchievements &achievements = m_achievements;
    store::StoreCloudSaves &cloud_saves = m_cloud_saves;
    store::StorePresence &presence = m_presence;
    return registrar.provide(store::kCoreService, PROVIDED_SERVICES[0].version, core) &&
           registrar.provide(store::kIapService, PROVIDED_SERVICES[1].version, iap) &&
           registrar.provide(store::kAchievementsService,
                              PROVIDED_SERVICES[2].version, achievements) &&
           registrar.provide(store::kCloudSavesService,
                              PROVIDED_SERVICES[3].version, cloud_saves) &&
           registrar.provide(store::kPresenceService, PROVIDED_SERVICES[4].version,
                              presence);
  }

  bool on_attach(nxe::ModuleContext &ctx) override {
    if (!ctx.schedule().try_define(
            PUMP_SYSTEM, nxe::sys::SystemFn([this](const nxe::sys::Context &) {
              m_platform.tick();
            }))) {
      nx::logw(log_store_egs, "system '{}' is already owned by another module",
                PUMP_SYSTEM);
      return false;
    }
    ctx.schedule().add(nxe::sys::Stage::Update, PUMP_SYSTEM);

    if (const std::optional<ServiceConfig> config = load_project_config();
        config.has_value()) {
      PlatformConfig platform_config;
      platform_config.product_id = config->product_id;
      platform_config.sandbox_id = config->sandbox_id;
      platform_config.deployment_id = config->deployment_id;
      platform_config.client_id = config->client_id;
      platform_config.client_secret = config->client_secret;
      platform_config.encryption_key = config->encryption_key;
      if (m_platform.initialize(platform_config))
        nx::logi(log_store_egs, "attached, Product ID {}", config->product_id);
      else
        nx::logw(log_store_egs, "EOS platform initialization failed");
    } else {
      nx::logi(log_store_egs, "no {} found; staying idle", kDefaultConfigPath);
    }

    return true;
  }

  void on_expose_scripts(nxe::script::Host &host, nxe::ModuleContext &) override {
    expose_store_egs_extras(host, m_leaderboards, m_overlay, m_mods);
  }

  void on_detach(nxe::ModuleContext &) override { m_platform.shutdown(); }

private:
  EgsPlatform m_platform;
  EgsCore m_core;
  EgsIap m_iap;
  EgsAchievements m_achievements;
  EgsCloudSaves m_cloud_saves;
  EgsPresence m_presence;

  // EGS-specific extras (leaderboards, social-overlay control, mods) -
  // never part of store_service.h's neutral interface, never registered
  // through ServiceRegistry (see store_egs_scripting.h).
  EgsLeaderboards m_leaderboards;
  EgsOverlay m_overlay;
  EgsMods m_mods;
};

} // namespace
} // namespace nxm::store_egs

NX_DECLARE_MODULE(store_egs, nxm::store_egs::StoreEgsModule)
