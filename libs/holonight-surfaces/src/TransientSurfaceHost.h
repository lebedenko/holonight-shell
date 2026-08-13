#pragma once

#include <QObject>
#include <QPointer>

#include <functional>
#include <holonight/wayland/layersurfacehost.h>
#include <memory>

class QQuickView;
class QScreen;

// Shared lifecycle for managers that own one short-lived layer surface at a time. Terminal host
// signals are queued and identity-guarded because QML animation callbacks and compositor events
// can race a replacement request.
class TransientSurfaceHost : public QObject {
  Q_OBJECT

 public:
  ~TransientSurfaceHost() override;
  TransientSurfaceHost(const TransientSurfaceHost&) = delete;
  TransientSurfaceHost& operator=(const TransientSurfaceHost&) = delete;
  TransientSurfaceHost(TransientSurfaceHost&&) = delete;
  TransientSurfaceHost& operator=(TransientSurfaceHost&&) = delete;

 protected:
  using HostFactory = std::function<std::unique_ptr<Holonight::Wayland::LayerSurfaceHost>()>;

  explicit TransientSurfaceHost(const char* log_tag, QObject* parent = nullptr);
  TransientSurfaceHost(const char* log_tag, HostFactory host_factory, QObject* parent = nullptr);

  bool openSurface(const Holonight::Wayland::LayerSurfaceSpec& spec);
  void closeSurface();
  void clearPendingSurface();

  [[nodiscard]] Holonight::Wayland::LayerSurfaceHost* host() const { return host_.get(); }
  [[nodiscard]] QQuickView* view() const;
  [[nodiscard]] QObject* rootObject() const;
  [[nodiscard]] bool hasSurface() const { return host_ != nullptr; }

  virtual bool openHost(Holonight::Wayland::LayerSurfaceHost& host, const Holonight::Wayland::LayerSurfaceSpec& spec);
  [[nodiscard]] virtual bool providerAvailable() const;
  virtual void onSurfaceConfigured();
  virtual void onSurfaceTerminated();

 private:
  void openPendingSurface();
  void removeCurrentHost(Holonight::Wayland::LayerSurfaceHost* expected_host);
  void handleAvailabilityChanged();
  void handleScreenRemoved(QScreen* screen);

  const char* log_tag_;
  HostFactory host_factory_;
  std::unique_ptr<Holonight::Wayland::LayerSurfaceHost> host_;
  Holonight::Wayland::LayerSurfaceSpec pending_spec_;
  QPointer<QScreen> requested_output_;
  bool pending_open_ = false;
};
