#pragma once

#include <QString>

#include <atomic>
#include <memory>
#include <optional>

// Reads CalDAV HTTP Basic Auth passwords from the system Secret Service (libsecret).
// Key format: the string set in password_keyring_key config field (e.g. "holonight-shell/caldav/work").
// If libsecret is unavailable, isServiceAvailable() returns false and all lookups return nullopt.
// Thread-safe: secret_password_lookup_sync() is re-entrant per libsecret documentation.
//
// The availability probe runs asynchronously via QtConcurrent (REQ-F-011): the constructor never
// blocks on secret_service_get_sync(). service_available_ is a heap-allocated, reference-counted
// atomic<bool> so the background probe lambda captures only a shared_ptr (never `this`) — the
// object may be destroyed while the probe is still in flight with zero dangling-pointer risk.
// This intentionally accepts a race window: isServiceAvailable()/lookupPassword() may read `false`
// for a brief period after construction even when the service is actually available. See
// docs/sdd/poc-remediation-phase3/DESIGN.md Item 5.
class LibsecretCredentialStorage {
 public:
  LibsecretCredentialStorage();
  ~LibsecretCredentialStorage() = default;

  LibsecretCredentialStorage(const LibsecretCredentialStorage&) = delete;
  LibsecretCredentialStorage& operator=(const LibsecretCredentialStorage&) = delete;
  LibsecretCredentialStorage(LibsecretCredentialStorage&&) = delete;
  LibsecretCredentialStorage& operator=(LibsecretCredentialStorage&&) = delete;

  [[nodiscard]] bool isServiceAvailable() const { return service_available_->load(std::memory_order_relaxed); }

  // Looks up the password for the given key. Returns nullopt on missing entry or error.
  // Password is never logged.
  [[nodiscard]] std::optional<QString> lookupPassword(const QString& key) const;

 private:
  std::shared_ptr<std::atomic<bool>> service_available_{std::make_shared<std::atomic<bool>>(false)};
};
