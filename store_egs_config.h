#pragma once

#include "core/foundation/strings/utf8_string.h"

#include <optional>

namespace nxm::store_egs {

/// VFS path StoreEgsModule::on_attach() checks automatically. A project
/// enables the module by cooking a file to this path (author it at
/// assets/config/store_egs.ini) with its game's EOS credentials from the
/// Epic Developer Portal - e.g.:
///
///   [store_egs]
///   product_id = ...
///   sandbox_id = ...
///   deployment_id = ...
///   client_id = ...
///   client_secret = ...
///   encryption_key = ...
///
/// The first five are required. Unlike Steam's public App ID, EOS's
/// client_secret is a real secret - this file likely shouldn't be committed
/// to a public repository. `encryption_key` (a 64-character hex string, EOS
/// requires exactly that shape) is optional - it only matters for cloud
/// saves (EOS_PlayerDataStorage encrypts at rest); a placeholder is used
/// when absent, which works but means a later real key change makes
/// existing saves unreadable, so set a real one before shipping if this
/// module's cloud saves are used.
inline constexpr nx::string_view kDefaultConfigPath = "/config/store_egs.ini";

struct ServiceConfig {
  nx::string product_id;
  nx::string sandbox_id;
  nx::string deployment_id;
  nx::string client_id;
  nx::string client_secret;
  nx::string encryption_key;
};

/// Reads and validates an INI file at @p path. Empty if the file does not
/// exist, or (with a logged warning) if it exists but fails to parse or is
/// missing a required field - callers should treat both the same way, as
/// "nothing to auto-configure with".
[[nodiscard]] std::optional<ServiceConfig>
load_project_config(nx::string_view path = kDefaultConfigPath);

}
