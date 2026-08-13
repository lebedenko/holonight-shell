#pragma once

#include <QObject>
#include <QPointer>

#include <functional>
#include <holonight/wayland/layersurfacehost.h>
#include <memory>

class QQuickView;
class QScreen;

struct PairedLayerSurfaceSpec {
  Holonight::Wayland::LayerSurfaceSpec dismiss;
  Holonight::Wayland::LayerSurfaceSpec content;
};

// Shared lifecycle for a short-lived content surface and its dismiss overlay. The overlay is
// opened first so the compositor stacks the content above it.
class PairedTransientSurfaceHost : public QObject {
  Q_OBJECT

 public:
  ~PairedTransientSurfaceHost() override;
  PairedTransientSurfaceHost(const PairedTransientSurfaceHost&) = delete;
  PairedTransientSurfaceHost& operator=(const PairedTransientSurfaceHost&) = delete;
  PairedTransientSurfaceHost(PairedTransientSurfaceHost&&) = delete;
  PairedTransientSurfaceHost& operator=(PairedTransientSurfaceHost&&) = delete;

 protected:
  using HostFactory = std::function<std::unique_ptr<Holonight::Wayland::LayerSurfaceHost>()>;

  explicit PairedTransientSurfaceHost(const char* log_tag, QObject* parent = nullptr);
  PairedTransientSurfaceHost(const char* log_tag, HostFactory host_factory, QObject* parent = nullptr);

  bool openPair(const PairedLayerSurfaceSpec& spec);
  void closePair();
  void clearPendingPair();

  [[nodiscard]] Holonight::Wayland::LayerSurfaceHost* contentHost() const { return content_host_.get(); }
  [[nodiscard]] Holonight::Wayland::LayerSurfaceHost* dismissHost() const { return dismiss_host_.get(); }
  [[nodiscard]] QQuickView* contentView() const;
  [[nodiscard]] QObject* contentRootObject() const;
  [[nodiscard]] bool hasPair() const { return content_host_ != nullptr && dismiss_host_ != nullptr; }
  [[nodiscard]] bool hasPendingPair() const { return pending_open_; }

  virtual bool openHost(Holonight::Wayland::LayerSurfaceHost& host, const Holonight::Wayland::LayerSurfaceSpec& spec);
  [[nodiscard]] virtual bool providerAvailable() const;
  virtual void onPairOpened();
  virtual void onPairConfigured();
  virtual void onPairTerminated();

  // Deterministic seams used by lifecycle tests.
  void providerAvailabilityChanged();
  void outputRemoved(QScreen* screen);

 private:
  void openPendingPair();
  void connectTerminalSignals(Holonight::Wayland::LayerSurfaceHost& host, quint64 generation);
  void terminateGeneration(quint64 generation, const QString& diagnostic = {});

  const char* log_tag_;
  HostFactory host_factory_;
  std::unique_ptr<Holonight::Wayland::LayerSurfaceHost> dismiss_host_;
  std::unique_ptr<Holonight::Wayland::LayerSurfaceHost> content_host_;
  PairedLayerSurfaceSpec pending_spec_;
  QPointer<QScreen> requested_output_;
  quint64 generation_ = 0;
  bool pending_open_ = false;
};
