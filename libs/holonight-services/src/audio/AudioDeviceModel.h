#pragma once

#include "AudioTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>

#include <cstdint>

class AudioDeviceModel : public QAbstractListModel {
  Q_OBJECT
  Q_CLASSINFO("QML.Element", "anonymous")

 public:
  enum class Role : uint16_t {
    DeviceId = Qt::UserRole + 1,
    Name,
    Description,
    Volume,
    Muted,
    IsDefault,
  };

  explicit AudioDeviceModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void applyAdd(const AudioDevice& device);
  void applyChange(const AudioDevice& device);
  void applyRemove(uint32_t device_id);
  void clear();

 private:
  QList<AudioDevice> devices_;
};
