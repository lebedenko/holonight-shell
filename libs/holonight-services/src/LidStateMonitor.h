#pragma once

#include "DbusPropertyClient.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class LidStateMonitor : public QObject {
  Q_OBJECT

 public:
  struct SkipInitTag {};
  static constexpr SkipInitTag SkipInit{};

  explicit LidStateMonitor(QObject* parent = nullptr);
  explicit LidStateMonitor(DbusPropertyClientPtr dbus, QObject* parent = nullptr);
  explicit LidStateMonitor(SkipInitTag /*tag*/, QObject* parent = nullptr);
  ~LidStateMonitor() override = default;

  LidStateMonitor(const LidStateMonitor&) = delete;
  LidStateMonitor& operator=(const LidStateMonitor&) = delete;
  LidStateMonitor(LidStateMonitor&&) = delete;
  LidStateMonitor& operator=(LidStateMonitor&&) = delete;

  void start();

  [[nodiscard]] bool lidPresent() const { return lid_present_; }
  [[nodiscard]] bool lidClosed() const { return lid_closed_; }

 Q_SIGNALS:
  void lidStateChanged(bool closed);

 private Q_SLOTS:
  void onPropertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);

 private:
  DbusPropertyClientPtr dbus_;
  bool lid_present_{false};
  bool lid_closed_{false};
  bool started_{false};
};
