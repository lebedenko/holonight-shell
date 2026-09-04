#pragma once

#include <QObject>
#include <QString>
#include <QtTypes>

class CompositorService;

class WindowActivationServer final : public QObject {
  Q_OBJECT

 public:
  static constexpr auto kServiceName = "org.holonight.Shell";
  static constexpr auto kObjectPath = "/org/holonight/Shell";
  static constexpr auto kInterfaceName = "org.holonight.Shell.WindowActivation1";

  explicit WindowActivationServer(CompositorService* compositor, QObject* parent = nullptr);
  ~WindowActivationServer() override;

  WindowActivationServer(const WindowActivationServer&) = delete;
  WindowActivationServer& operator=(const WindowActivationServer&) = delete;
  WindowActivationServer(WindowActivationServer&&) = delete;
  WindowActivationServer& operator=(WindowActivationServer&&) = delete;

  [[nodiscard]] bool start();
  [[nodiscard]] bool requestWindowActivation(const QList<quint32>& process_lineage, const QString& title_hint);
  [[nodiscard]] CompositorService* compositor() const { return compositor_; }

 private:
  CompositorService* compositor_;
  bool service_registered_{false};
  bool object_registered_{false};
  bool start_attempted_{false};
};
