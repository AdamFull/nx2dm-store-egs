#pragma once

namespace nxe::script {
class Host;
}

namespace nxm::store_egs {

class EgsLeaderboards;
class EgsOverlay;
class EgsMods;

/// The EGS-only `host.store_egs_*` surface (leaderboards, social-overlay
/// control, mods) - deliberately separate from store/store_scripting.cpp,
/// which stays neutral-only. Unlike the neutral surface, these three are
/// captured by direct reference rather than looked up through
/// ServiceRegistry: nothing outside store_egs itself will ever need to find
/// them, so there's no "which backend provides this" question to resolve.
void expose_store_egs_extras(nxe::script::Host &host,
                              EgsLeaderboards &leaderboards,
                              EgsOverlay &overlay, EgsMods &mods);

}
