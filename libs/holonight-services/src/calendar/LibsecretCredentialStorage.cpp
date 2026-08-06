#include "LibsecretCredentialStorage.h"

#include <QFuture>
#include <QLoggingCategory>
#include <QtConcurrent/QtConcurrentRun>

// libsecret headers define `signals` via GLib, which conflicts with Qt's keyword.
// Push/pop the macro around the include to prevent the clash.
#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libsecret/secret.h>
}
#pragma pop_macro("signals")

Q_LOGGING_CATEGORY(lcCredStorage, "holonight.calendar.credentials")

namespace {

// SecretSchema used for all CalDAV password lookups. The "key" attribute holds the full
// password_keyring_key string (e.g. "holonight-shell/caldav/work").
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
const SecretSchema kSchema = {
    .name = "io.github.holonight.shell.token",
    .flags = SECRET_SCHEMA_NONE,
    .attributes =
        {
            {.name = "key", .type = SECRET_SCHEMA_ATTRIBUTE_STRING},
            {.name = nullptr, .type = SECRET_SCHEMA_ATTRIBUTE_STRING},
        },
};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

LibsecretCredentialStorage::LibsecretCredentialStorage() {
  // service_available_ is already default-constructed to a heap atomic<bool>(false) via the
  // in-class initializer; this constructor only schedules the probe and returns — it performs no
  // blocking I/O itself (REQ-F-011). The lambda captures a shared_ptr to the flag, not `this`, so
  // the probe is safe to outlive this object.
  std::shared_ptr<std::atomic<bool>> flag = service_available_;
  [[maybe_unused]] const QFuture<void> probe_future = QtConcurrent::run([flag]() {
    GError* error = nullptr;
    SecretService* service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (error != nullptr) {
      qCInfo(lcCredStorage) << "Secret Service unavailable:" << error->message;
      g_error_free(error);
      return;
    }
    if (service == nullptr) {
      qCInfo(lcCredStorage) << "Secret Service unavailable: no service instance";
      return;
    }
    g_object_unref(service);
    flag->store(true, std::memory_order_relaxed);
  });
}

std::optional<QString> LibsecretCredentialStorage::lookupPassword(const QString& key) const {
  if (!service_available_->load(std::memory_order_relaxed)) {
    return std::nullopt;
  }
  GError* error = nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  gchar* password = secret_password_lookup_sync(&kSchema, nullptr, &error, "key", key.toUtf8().constData(), nullptr);
  if (error != nullptr) {
    qCWarning(lcCredStorage) << "Keyring lookup failed for key" << key << ":" << error->message;
    g_error_free(error);
    return std::nullopt;
  }
  if (password == nullptr) {
    qCWarning(lcCredStorage) << "No keyring entry found for key" << key;
    return std::nullopt;
  }
  QString result = QString::fromUtf8(password);
  secret_password_free(password);
  return result;
}
