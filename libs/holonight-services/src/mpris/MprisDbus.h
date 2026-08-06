#pragma once

#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

class QObject;

// Test seam: abstracts session-bus D-Bus calls so MprisService unit tests never touch the real
// bus (REQ-NF-001). Blocking discovery/property reads, two signal-subscription pairs, and three
// void fire-and-forget command methods (REQ-F-013).
class IMprisDBus {
 public:
  virtual ~IMprisDBus() = default;
  IMprisDBus() = default;
  IMprisDBus(const IMprisDBus&) = delete;
  IMprisDBus& operator=(const IMprisDBus&) = delete;
  IMprisDBus(IMprisDBus&&) = delete;
  IMprisDBus& operator=(IMprisDBus&&) = delete;

  // Discovery (REQ-F-001, REQ-C-003)
  [[nodiscard]] virtual QStringList listNames() = 0;  // blocking; startup snapshot
  virtual bool connectNameOwnerChanged(QObject* receiver, const char* slot) = 0;

  // Per-player property read + live subscription (REQ-F-002, REQ-F-003)
  [[nodiscard]] virtual QVariantMap getAllRootProperties(const QString& service) = 0;                   // blocking
  [[nodiscard]] virtual std::optional<QVariantMap> getAllPlayerProperties(const QString& service) = 0;  // blocking
  virtual bool connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) = 0;
  virtual void disconnectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) = 0;

  // Player.Seeked(x) subscription (REQ-F-013, REQ-F-018, REQ-F-021(c)) — mirrors the
  // PropertiesChanged pair above. `x` is the corrected position in microseconds.
  virtual bool connectSeeked(const QString& service, QObject* receiver, const char* slot) = 0;
  virtual void disconnectSeeked(const QString& service, QObject* receiver, const char* slot) = 0;

  // Fire-and-forget commands (REQ-F-013). void return type — there is nothing to await by
  // construction, so a caller cannot accidentally attach a watcher and start reacting to replies.
  virtual void asyncPlayPause(const QString& service) = 0;
  virtual void asyncNext(const QString& service) = 0;
  virtual void asyncPrevious(const QString& service) = 0;
};

// Production implementation: delegates every call to the real session bus.
class SystemMprisDBus final : public IMprisDBus {
 public:
  static constexpr int kCallTimeoutMs = 500;

  [[nodiscard]] QStringList listNames() override;
  bool connectNameOwnerChanged(QObject* receiver, const char* slot) override;
  [[nodiscard]] QVariantMap getAllRootProperties(const QString& service) override;
  [[nodiscard]] std::optional<QVariantMap> getAllPlayerProperties(const QString& service) override;
  bool connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) override;
  void disconnectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) override;
  bool connectSeeked(const QString& service, QObject* receiver, const char* slot) override;
  void disconnectSeeked(const QString& service, QObject* receiver, const char* slot) override;
  void asyncPlayPause(const QString& service) override;
  void asyncNext(const QString& service) override;
  void asyncPrevious(const QString& service) override;
};

// Test double: in-memory player table, synchronous signal injection, no dbus-daemon required
// (REQ-NF-001).
class FakeMprisDBus final : public IMprisDBus {
 public:
  // Test setup: register the two property interfaces independently, matching real MPRIS.
  void seedPlayer(const QString& service, const QVariantMap& root_properties, const QVariantMap& player_properties);
  void seedPlayer(const QString& service, const QVariantMap& combined_properties);
  void removePlayer(const QString& service);

  // Test-driven signal injection — synchronous, no event loop spin required.
  void emitNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);
  void emitPropertiesChanged(const QString& service, const QString& interface_name, const QVariantMap& changed,
                             const QStringList& invalidated = {});
  void emitPlayerPropertiesChanged(const QString& service, const QVariantMap& changed,
                                   const QStringList& invalidated = {});
  void emitSeeked(const QString& service, qint64 positionUs);
  void setConnectPropertiesChangedResult(bool result) { connect_properties_changed_result_ = result; }
  void setPlayerPropertiesReadFailure(bool fail) { player_properties_read_failure_ = fail; }

  // Spies for REQ-F-013/014/015 assertions.
  [[nodiscard]] QStringList playPauseCalls() const { return play_pause_calls_; }
  [[nodiscard]] QStringList nextCalls() const { return next_calls_; }
  [[nodiscard]] QStringList previousCalls() const { return previous_calls_; }
  [[nodiscard]] bool propertiesRequestedBeforeSubscription() const { return properties_requested_before_subscription_; }
  // Reconciliation-timer test hook (REQ-F-022): counts getAllPlayerProperties() calls so tests can
  // verify a reconcile tick actually issues a fresh Position/Rate read.
  [[nodiscard]] int getAllPlayerPropertiesCallCount() const { return get_all_player_properties_call_count_; }

  // IMprisDBus
  [[nodiscard]] QStringList listNames() override;
  bool connectNameOwnerChanged(QObject* receiver, const char* slot) override;
  [[nodiscard]] QVariantMap getAllRootProperties(const QString& service) override;
  [[nodiscard]] std::optional<QVariantMap> getAllPlayerProperties(const QString& service) override;
  bool connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) override;
  void disconnectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) override;
  bool connectSeeked(const QString& service, QObject* receiver, const char* slot) override;
  void disconnectSeeked(const QString& service, QObject* receiver, const char* slot) override;
  void asyncPlayPause(const QString& service) override { play_pause_calls_.append(service); }
  void asyncNext(const QString& service) override { next_calls_.append(service); }
  void asyncPrevious(const QString& service) override { previous_calls_.append(service); }

 private:
  QHash<QString, QVariantMap> root_properties_;
  QHash<QString, QVariantMap> player_properties_;
  QObject* name_owner_receiver_{nullptr};
  const char* name_owner_slot_{nullptr};
  QHash<QString, QPair<QObject*, const char*>> prop_receivers_;
  QHash<QString, QPair<QObject*, const char*>> seeked_receivers_;
  bool connect_properties_changed_result_{true};
  bool properties_requested_before_subscription_{false};
  int get_all_player_properties_call_count_{0};
  bool player_properties_read_failure_{false};
  QStringList play_pause_calls_;
  QStringList next_calls_;
  QStringList previous_calls_;
};
