#pragma once

#include "HyprlandIpcClient.h"

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <memory>

class KeyboardLayoutService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QString layoutCode READ layoutCode NOTIFY layoutCodeChanged)
  Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutNameChanged)

 public:
  explicit KeyboardLayoutService(QObject* parent = nullptr);
  explicit KeyboardLayoutService(HyprlandIpcTransportPtr ipc_client, QObject* parent = nullptr);
  ~KeyboardLayoutService() override = default;

  KeyboardLayoutService(const KeyboardLayoutService&) = delete;
  KeyboardLayoutService& operator=(const KeyboardLayoutService&) = delete;
  KeyboardLayoutService(KeyboardLayoutService&&) = delete;
  KeyboardLayoutService& operator=(KeyboardLayoutService&&) = delete;

  void start();

  [[nodiscard]] QString layoutCode() const { return layout_code_; }

  // REQ-C-014. Full layout name as reported by Hyprland (e.g. "English (US)"), retained alongside
  // the derived two-letter code for consumers that want to spell it out.
  [[nodiscard]] QString layoutName() const { return layout_name_; }

 Q_SIGNALS:
  void layoutCodeChanged();
  void layoutNameChanged();

 private:
  void connectSocket();
  void queryCurrentLayout();
  void processEventLine(const QByteArray& line);
  void onEventSocketConnected();
  void onCommandFinished(const QByteArray& response, bool success);
  void setLayoutName(const QString& value);
  void setLayoutCode(const QString& value);

  HyprlandIpcTransportPtr ipc_client_;
  QString layout_code_;
  QString layout_name_;
  bool started_{false};
};
