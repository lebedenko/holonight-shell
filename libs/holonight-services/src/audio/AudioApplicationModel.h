#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

#include <AudioController.h>
#include <optional>

// Shell presentation policy; the provider retains independent per-stream controls.
class AudioApplicationModel : public QAbstractListModel {
  Q_OBJECT
  QML_ANONYMOUS

 public:
  enum Role { ApplicationKey = Qt::UserRole + 1, Application, IconName, StreamCount, Subtitle, Volume, Muted };
  explicit AudioApplicationModel(HoloNight::System::AudioController* controller);
  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  Q_INVOKABLE void setVolume(const QString& key, int percent);
  Q_INVOKABLE void setMuted(const QString& key, bool muted);

 private:
  struct Group {
    QString key;
    QString name;
    QString fallback_name;
    QString binary;
    QString icon;
    QString subtitle;
    QList<uint32_t> members;
    int volume{0};
    bool muted{true};
    std::optional<int> requested_volume;
    std::optional<bool> requested_mute;
  };
  void synchronize();
  HoloNight::System::AudioController* controller_;
  QList<Group> groups_;
};
