#pragma once
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantMap>

#include <StorageController.h>
#include <cstdint>

class StorageService : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
  Q_PROPERTY(QString confirmationText READ confirmationText NOTIFY changed)
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
    CanPowerOff
  };
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const { return static_cast<int>(rows_.size()); }
  [[nodiscard]] QString errorMessage() const { return error_message_; }
  [[nodiscard]] QString confirmationText() const { return confirmation_text_; }
  Q_INVOKABLE [[nodiscard]] QString driveLabel(const QString& targetId) const;
  Q_INVOKABLE void mount(const QString& targetId);
  Q_INVOKABLE void unmount(const QString& targetId);
  Q_INVOKABLE void eject(const QString& targetId);
  Q_INVOKABLE void requestPowerOff(const QString& targetId);
  Q_INVOKABLE void confirmPowerOff();
  Q_INVOKABLE void cancelPowerOff();
 signals:
  void changed();

 private:
  void refresh();
  void appendOpticalDrives(QList<QVariantMap>& rows) const;
  bool visibleTarget(const QString& targetId, const char* capability) const;
  HoloNight::System::StorageController* controller_;
  QList<QVariantMap> rows_;
  QString error_message_;
  QString confirmation_text_;
  QString confirmation_drive_;
  QStringList confirmation_scope_;
};
