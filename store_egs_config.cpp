#include "store_egs/store_egs_config.h"

#include "core/foundation/diagnostics/log.h"
#include "core/foundation/serialization/async_documents.h"

namespace nxm::store_egs {
namespace {

const nx::log::Category log_store_egs = nx::log::category("store_egs");

[[nodiscard]] const nx::string *
require(const nx::ini::Document &doc, const nx::string_view key,
        const nx::string_view path, bool &ok) {
  const nx::string *const value = doc.find("store_egs", key);
  if (value == nullptr) {
    nx::logw(log_store_egs, "config: '{}' is missing [store_egs] {}", path,
              key);
    ok = false;
  }
  return value;
}

} // namespace

std::optional<ServiceConfig> load_project_config(nx::vfs::AsyncIoService &io,
                                                 const nx::string_view path) {
  // Its cooked form where there is one, as a Shipping build has.
  const auto parsed = nx::ini::load(io, path).get();
  if (!parsed) {
    const nx::ini::LoadError &error = parsed.error();
    if (error.kind == nx::ini::LoadErrorKind::Invalid)
      nx::logw(log_store_egs, "config: {}:{}:{}: {}", path, error.parse.line,
               error.parse.column, error.message());
    else if (error.kind != nx::ini::LoadErrorKind::NotFound)
      nx::logw(log_store_egs, "config: could not read '{}': {}", path,
               error.message());
    return {};
  }

  bool ok = true;
  const nx::string *const product_id = require(*parsed, "product_id", path, ok);
  const nx::string *const sandbox_id = require(*parsed, "sandbox_id", path, ok);
  const nx::string *const deployment_id =
      require(*parsed, "deployment_id", path, ok);
  const nx::string *const client_id = require(*parsed, "client_id", path, ok);
  const nx::string *const client_secret =
      require(*parsed, "client_secret", path, ok);
  if (!ok)
    return {};

  ServiceConfig config;
  config.product_id = *product_id;
  config.sandbox_id = *sandbox_id;
  config.deployment_id = *deployment_id;
  config.client_id = *client_id;
  config.client_secret = *client_secret;
  if (const nx::string *const key = parsed->find("store_egs", "encryption_key"))
    config.encryption_key = *key;
  else
    // A valid-shaped (64 hex chars) placeholder - EOS requires this exact
    // length to configure PlayerDataStorage encryption at all, but this
    // module works without one being set to a real value until cloud saves
    // are actually used.
    config.encryption_key = nx::string(64, '0');
  return config;
}
}
