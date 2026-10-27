#pragma once
#include <QQmlEngine>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <BluetoothController.h>
#include <memory>
class BluetoothService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QVariantList adapters READ adapters NOTIFY changed)
  Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
  Q_PROPERTY(QVariantMap selectedAdapter READ selectedAdapter NOTIFY changed)
  Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)
  Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
  Q_PROPERTY(QVariantMap interaction READ interaction NOTIFY changed)
  Q_PROPERTY(int connectedCount READ connectedCount NOTIFY changed)
  Q_PROPERTY(QString tooltip READ tooltip NOTIFY changed)
 public:
  explicit BluetoothService(QObject* parent = nullptr);
  ~BluetoothService() override;
  BluetoothService(HoloNight::System::BluetoothBackend* backend, QObject* parent);
  QVariantList adapters() const;
  QVariantList devices() const;
  QVariantMap selectedAdapter() const;
  QString selectedId() const { return selected_; }
  bool scanning() const { return scan_requested_; }
  bool available() const { return controller_->available(); }
  QString errorMessage() const { return error_; }
  QVariantMap interaction() const { return interaction_; }
  int connectedCount() const;
  QString tooltip() const;
  Q_INVOKABLE void registerAnchor(const QString& monitor, int x, int width) { anchors_.insert(monitor, {x, width}); }
  Q_INVOKABLE void unregisterAnchor(const QString& monitor) { anchors_.remove(monitor); }
  std::optional<QPair<int, int>> anchor(const QString& monitor) const {
    if (anchors_.contains(monitor)) return anchors_.value(monitor);
    return {};
  }
  Q_INVOKABLE void selectAdapter(const QString& id);
  Q_INVOKABLE void setActive(bool active);
  Q_INVOKABLE void scan(bool enabled);
  Q_INVOKABLE void power(bool enabled);
  Q_INVOKABLE void discoverable(bool enabled);
  Q_INVOKABLE void renameAdapter(const QString& alias);
  Q_INVOKABLE void action(const QString& id, const QString& action, const QVariant& value = {});
  Q_INVOKABLE void respond(bool accept, const QString& input = {});
  Q_INVOKABLE void cancelPairing();
  Q_INVOKABLE void clearError() {
    error_.clear();
    emit changed();
  }
  void setDiscoveryReleaseDelay(int milliseconds) { release_.setInterval(milliseconds); }
  void requestDefaultAgent() { controller_->requestDefaultAgent(); }
 signals:
  void changed();
  void pairingRequested();

 private:
  void update();
  void reconcileDiscovery();
  void completed(const HoloNight::System::BluetoothResult& result);
  std::unique_ptr<HoloNight::System::BluetoothController> controller_;
  QString selected_, error_, pairing_, chain_request_, discovery_request_, discovery_target_;
  HoloNight::System::BluetoothOperation chain_operation_ = HoloNight::System::BluetoothOperation::Pair;
  bool active_ = false, scan_requested_ = false, explicit_selection_ = false;
  QTimer release_;
  QSet<QString> owned_;
  QVariantMap interaction_;
  mutable QStringList display_order_;
  QHash<QString, QPair<int, int>> anchors_;
};
