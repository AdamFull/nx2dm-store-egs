#include "framework/nxtest.h"

#include "app/engine.h"
#include "core/foundation/platform/filesystem.h"
#include "script/luau/luau_backend.h"
#include "script/luau/luau_bindings.h"
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
};

} // namespace

TEST_CASE("store_egs scripting: every service is exposed as script-services.json "
          "declares it") {
  const Exposed exposed;
  const auto manifest = nx::fs::file_read_text(
      nx::fs::path_view(NX_MODULE_SERVICES_MANIFEST));
  REQUIRE(manifest);

  nx::string error;
  if (!script::luau_manifest_agrees(manifest.value(), exposed.services, error))
    FAIL(error.c_str());
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

// No EOS platform runs here, so every list is empty - but each arrives as a
// table a script can walk, agreeing with the count beside it.
TEST_CASE("store_egs scripting: the lists come back as tables") {
  EgsPlatform platform;
  EgsPresence presence{platform};
  EgsLeaderboards leaderboards{platform};
  EgsOverlay overlay{platform, presence};
  EgsMods mods{platform};
  script::Host host;
  REQUIRE(host.set_backend(script::luau_backend()));
  expose_store_egs_extras(host, leaderboards, overlay, mods);
  REQUIRE(host.bind());
  const nx::string_view source = R"(
local entries = host.store_egs_leaderboard_entries()
assert(#entries == host.store_egs_leaderboard_entry_count(), "entries")
local installed = host.store_egs_mods_installed()
assert(#installed == host.store_egs_mods_installed_count(), "installed")
local available = host.store_egs_mods_available()
assert(#available == host.store_egs_mods_available_count(), "available")
return {}
)";
  CHECK(host.load("egs_lists",
                  {reinterpret_cast<const std::byte *>(source.data()),
                   source.size()}));
}
