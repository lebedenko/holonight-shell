#pragma once

#include "KeyboardLayoutProvider.h"
#include "ShellHyprlandIpcClient.h"

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <memory>

class HyprlandLayoutProvider : public KeyboardLayoutProvider {
  Q_OBJECT
  Q_PROPERTY(QString layoutCode READ layoutCode NOTIFY layoutCodeChanged)
  Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutNameChanged)

 public:
  explicit HyprlandLayoutProvider(QObject* parent = nullptr);
  explicit HyprlandLayoutProvider(ShellHyprlandIpcTransportPtr ipc_client, QObject* parent = nullptr);
  ~HyprlandLayoutProvider() override = default;

  HyprlandLayoutProvider(const HyprlandLayoutProvider&) = delete;
  HyprlandLayoutProvider& operator=(const HyprlandLayoutProvider&) = delete;
  HyprlandLayoutProvider(HyprlandLayoutProvider&&) = delete;
  HyprlandLayoutProvider& operator=(HyprlandLayoutProvider&&) = delete;

  void start() override;

  [[nodiscard]] QString layoutCode() const override { return layout_code_; }

  // REQ-C-014. Full layout name as reported by Hyprland (e.g. "English (US)"), retained alongside
  // the derived two-letter code for consumers that want to spell it out.
  [[nodiscard]] QString layoutName() const override { return layout_name_; }

 private:
  void connectSocket();
  void queryCurrentLayout();
  void processEventLine(const QByteArray& line);
  void onEventSocketConnected();
  void onCommandFinished(const QByteArray& response, bool success);
  void setLayoutName(const QString& value);
  void setLayoutCode(const QString& value);

  ShellHyprlandIpcTransportPtr ipc_client_;
  QString layout_code_;
  QString layout_name_;
  bool started_{false};
};
