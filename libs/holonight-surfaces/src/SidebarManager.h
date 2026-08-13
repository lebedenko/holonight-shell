#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <holonight/wayland/layersurfacehost.h>
#include <memory>

class QScreen;

// Owns one short-lived sidebar host per open or closing monitor. Closing hosts remain alive until
// QML reports that their animation finished; host identity and generation guard every asynchronous
// terminal callback.
class SidebarManager : public QObject {
  Q_OBJECT

 public:
  explicit SidebarManager(QObject* parent = nullptr);
  ~SidebarManager() override;

  SidebarManager(const SidebarManager&) = delete;
  SidebarManager& operator=(const SidebarManager&) = delete;
  SidebarManager(SidebarManager&&) = delete;
  SidebarManager& operator=(SidebarManager&&) = delete;

  void start();
  [[nodiscard]] Q_INVOKABLE bool isOpen(const QString& monitor_name) const;
  [[nodiscard]] Q_INVOKABLE static bool isKnownMonitor(const QString& monitor_name);
  Q_INVOKABLE void toggle(const QString& monitor_name);
  Q_INVOKABLE void close(const QString& monitor_name);
  Q_INVOKABLE void closeAll();
  Q_INVOKABLE void onClosingAnimationFinished(const QString& monitor_name);
  Q_INVOKABLE void onContentHeightChanged(const QString& monitor_name, int height);
  Q_INVOKABLE void onCurrentTabChanged(const QString& monitor_name, int tab_index);
  [[nodiscard]] Q_INVOKABLE int currentTabForMonitor(const QString& monitor_name) const;

 Q_SIGNALS:
  void sidebarOpened(const QString& monitor_name);
  void sidebarClosed(const QString& monitor_name);
  void currentTabChanged(const QString& monitor_name, int tab_index);

 protected:
  using HostFactory = std::function<std::unique_ptr<Holonight::Wayland::LayerSurfaceHost>()>;
  SidebarManager(HostFactory host_factory, QObject* parent = nullptr);
  virtual bool openHost(Holonight::Wayland::LayerSurfaceHost& host, const Holonight::Wayland::LayerSurfaceSpec& spec);
  [[nodiscard]] virtual bool providerAvailable() const;
  [[nodiscard]] virtual QScreen* screenForName(const QString& monitor_name) const;
  void handleProviderAvailabilityChanged();
  void handleOutputRemoved(const QString& monitor_name);

 private:
  struct SidebarSurface {
    std::shared_ptr<Holonight::Wayland::LayerSurfaceHost> host;
    quint64 generation{0};
    bool closing{false};
  };

  void openOnMonitor(const QString& monitor_name);
  void closeOnMonitor(const QString& monitor_name);
  void destroySurface(const QString& monitor_name, quint64 generation);
  void handleHostTerminated(const QString& monitor_name, quint64 generation,
                            Holonight::Wayland::LayerSurfaceHost* expected_host);
  [[nodiscard]] int boundedHeight(const QString& monitor_name, int requested_height) const;
  [[nodiscard]] QObject* rootObject(const QString& monitor_name) const;

  HostFactory host_factory_;
  bool started_{false};
  quint64 next_generation_{0};
  QHash<QString, bool> open_state_;
  QHash<QString, int> stored_heights_;
  QHash<QString, int> current_tabs_;
  QHash<QString, SidebarSurface> surfaces_;
};
