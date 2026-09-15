#include "framework/nxtest.h"

#include "app/engine.h"
#include "script/script_host.h"
#include "store_egs/store_egs_leaderboards.h"
#include "store_egs/store_egs_mods.h"
#include "store_egs/store_egs_overlay.h"
#include "store_egs/store_egs_scripting.h"
#include "store_egs/store_egs_services.h"

namespace {

using namespace nxm::store_egs;
namespace script = nxe::script;

struct Exposed {
  EgsPlatform platform;
  EgsPresence presence{platform};
  EgsLeaderboards leaderboards{platform};
  EgsOverlay overlay{platform, presence};
  EgsMods mods{platform};
  script::Host host;
  nx::vector<script::Host::ServiceInfo> services;

  Exposed() {
    expose_store_egs_extras(host, leaderboards, overlay, mods);
    services = host.services();
  }

  [[nodiscard]] const script::Host::ServiceInfo *
  find(const nx::string_view name) const {
    for (const script::Host::ServiceInfo &one : services)
      if (one.name == name)
        return &one;
    return nullptr;
  }
};

} // namespace

// This is the one place a mismatch between what store_egs_scripting.cpp
// actually registers and what modules/store_egs/script-services.json
// declares to Luau would show up - see test_modio_scripting.cpp's identical
// role for modio. No backend is registered in this harness (no live EOS
// session runs), so every callable here just exercises its own "no session"
// refusal path, not real EOS behavior.
TEST_CASE("store_egs scripting: every service is exposed with the shape a "
          "script is told about") {
  const Exposed exposed;

  static constexpr struct {
    nx::string_view name;
    nx::string_view signature;
  } WANT[] = {
      {"store_egs_leaderboard_download", "(string)->(boolean)"},
      {"store_egs_leaderboard_download_pending", "()->(boolean)"},
      {"store_egs_leaderboard_entry_count", "()->(number)"},
      {"store_egs_leaderboard_entry_rank", "(number)->(number)"},
      {"store_egs_leaderboard_entry_score", "(number)->(number)"},
      {"store_egs_leaderboard_entry_name", "(number)->(string)"},
      {"store_egs_overlay_show_friends", "()->(boolean)"},
      {"store_egs_overlay_hide_friends", "()->(boolean)"},
      {"store_egs_overlay_friends_visible", "()->(boolean)"},
      {"store_egs_overlay_show_block_player", "(number)->(boolean)"},
      {"store_egs_overlay_show_report_player", "(number)->(boolean)"},
      {"store_egs_overlay_show_native_profile", "(number)->(boolean)"},
      {"store_egs_overlay_pause_social_overlay", "(boolean)->(boolean)"},
      {"store_egs_overlay_social_overlay_paused", "()->(boolean)"},
      {"store_egs_mods_refresh_installed", "()->(boolean)"},
      {"store_egs_mods_refresh_available", "()->(boolean)"},
      {"store_egs_mods_installed_count", "()->(number)"},
      {"store_egs_mods_installed_title", "(number)->(string)"},
      {"store_egs_mods_installed_version", "(number)->(string)"},
      {"store_egs_mods_available_count", "()->(number)"},
      {"store_egs_mods_available_title", "(number)->(string)"},
      {"store_egs_mods_available_version", "(number)->(string)"},
      {"store_egs_mods_install", "(number)->(boolean)"},
      {"store_egs_mods_install_pending", "()->(boolean)"},
      {"store_egs_mods_install_error", "()->(string)"},
      {"store_egs_mods_uninstall", "(number)->(boolean)"},
      {"store_egs_mods_uninstall_pending", "()->(boolean)"},
      {"store_egs_mods_uninstall_error", "()->(string)"},
      {"store_egs_mods_update", "(number)->(boolean)"},
      {"store_egs_mods_update_pending", "()->(boolean)"},
      {"store_egs_mods_update_error", "()->(string)"},
  };

  CHECK(exposed.services.size() == nx::array_size(WANT));
  for (const auto &want : WANT) {
    const script::Host::ServiceInfo *const found = exposed.find(want.name);
    REQUIRE(found != nullptr);
    CHECK(found->signature == want.signature);
  }
}

TEST_CASE("store_egs scripting: the module hands them over on its own") {
  std::unique_ptr<nxe::Module> found;
  for (const nxe::ModuleFactory factory : nxe::enabled_module_factories()) {
    std::unique_ptr<nxe::Module> module = factory();
    if (module != nullptr && module->name() == "store_egs")
      found = std::move(module);
  }
  REQUIRE(found != nullptr);

  nxe::Engine engine{nxe::Game{}};
  nxe::ModuleContext ctx{engine};
  script::Host host;
  found->on_expose_scripts(host, ctx);

  script::Host direct;
  EgsPlatform platform;
  EgsPresence presence{platform};
  EgsLeaderboards leaderboards{platform};
  EgsOverlay overlay{platform, presence};
  EgsMods mods{platform};
  expose_store_egs_extras(direct, leaderboards, overlay, mods);
  CHECK(host.exposed_count() == direct.exposed_count());
  CHECK(host.exposed_count() > 0u);
}
