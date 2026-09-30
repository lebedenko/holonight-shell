#pragma once
#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QSet>
#include <QVariantMap>

#include <StorageController.h>
#include <cstdint>

class StorageService : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(int deviceCount READ deviceCount NOTIFY changed)
  Q_PROPERTY(bool hasMounted READ hasMounted NOTIFY changed)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
 public:
  explicit StorageService(QObject* parent = nullptr);
  StorageService(HoloNight::System::StorageController* controller, QObject* parent = nullptr);
  enum class Role : uint16_t {
    TargetId = Qt::UserRole + 1,
    DriveId,
    Name,
    DriveName,
    State,
    Mounted,
    Busy,
    CanMount,
    CanUnmount,
    CanEject,
    CanPowerOff,
    UsedBytes,
    TotalBytes,
    FreeBytes,
    OperationText,
    ErrorText
  };
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const { return static_cast<int>(rows_.size()); }
  [[nodiscard]] int deviceCount() const;
  [[nodiscard]] bool hasMounted() const;
  [[nodiscard]] QString errorMessage() const { return error_message_; }
  Q_INVOKABLE [[nodiscard]] QString driveLabel(const QString& targetId) const;
  Q_INVOKABLE [[nodiscard]] QString driveIconName(const QString& driveId) const;
  Q_INVOKABLE [[nodiscard]] QString driveSubtitle(const QString& driveId) const;
  Q_INVOKABLE [[nodiscard]] QString driveCapacityText(const QString& driveId) const;
  Q_INVOKABLE [[nodiscard]] QString driveOperationText(const QString& driveId) const;
  Q_INVOKABLE [[nodiscard]] QString driveErrorText(const QString& driveId) const;
  Q_INVOKABLE [[nodiscard]] bool driveCanEject(const QString& driveId) const;
  Q_INVOKABLE [[nodiscard]] bool driveCanPowerOff(const QString& driveId) const;
  Q_INVOKABLE void mount(const QString& targetId);
  Q_INVOKABLE void unmount(const QString& targetId);
  Q_INVOKABLE void eject(const QString& targetId);
  Q_INVOKABLE void powerOff(const QString& targetId);
  Q_INVOKABLE void retry(const QString& targetId);
  Q_INVOKABLE void openVolume(const QString& targetId);
  Q_INVOKABLE void openInFiles() const;
  Q_INVOKABLE void showAllDevices() const;
 signals:
  void changed();

 private:
  enum class DeviceKind { Optical, Thumb, Flash, HardDisk, SolidState, Unknown };
  static DeviceKind classifyDevice(const HoloNight::System::StorageDrive& drive);
  struct ErrorEntry {
    HoloNight::System::StorageOperation operation = HoloNight::System::StorageOperation::Mount;
    QString message;
    quint64 generation = 0;
  };
  void refresh();
  void appendOpticalDrives(QList<QVariantMap>& rows) const;
  bool visibleTarget(const QString& targetId, const char* capability) const;
  void beginOperation(const QString& targetId, HoloNight::System::StorageOperation operation);
  [[nodiscard]] QString operationTextFor(const QString& targetId) const;
  [[nodiscard]] QString errorTextFor(const QString& targetId) const;
  void notifyNewlyConnectedDrives(const QSet<QString>& current_drive_ids);
  void sendStorageNotification(const QString& summary) const;
  void sendSafeToRemoveNotification(const QString& label) const;
  void sendDriveConnectedNotification(const QString& driveId) const;
  HoloNight::System::StorageController* controller_;
  QList<QVariantMap> rows_;
  QString error_message_;
  QHash<QString, HoloNight::System::StorageOperation> in_flight_ops_;
  QHash<QString, QString> removal_labels_;
  QHash<QString, ErrorEntry> last_errors_;
  QSet<QString> pending_open_after_mount_;
  QSet<QString> known_drive_ids_;
  bool known_drive_ids_seeded_ = false;
  quint64 next_error_generation_ = 0;
};
