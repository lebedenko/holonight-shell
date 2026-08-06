#pragma once

#include "CalendarTypes.h"

#include <QAbstractListModel>
#include <QList>

#include <cstdint>

// QAbstractListModel wrapper around a QList<CalendarEvent> for QML consumption.
// setEvents() replaces the entire list atomically with begin/endResetModel.
class CalendarEventModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Roles : std::uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
    EventIdRole = Qt::UserRole + 1,
    TitleRole,
    StartTimeRole,
    EndTimeRole,
    IsAllDayRole,
    ProviderRole,
    DescriptionRole,
    LocationRole,
    RruleRole,
    DurationRole,
    AccessClassRole,
    CreatedRole,
    LastModifiedRole,
    OrganizerRole,
    AttendeesRole,
    CategoriesRole,
    StatusRole,
    TransparencyRole,
    UrlRole,
    GeoRole,
    SequenceRole,
    RecurrenceIdRole,
    ExdatesRole,
    RdatesRole,
    AttachmentsRole,
    CommentsRole,
    ContactsRole,
    RelatedToRole,
    ResourcesRole,
    AlarmsRole,
  };
  Q_ENUM(Roles)

  explicit CalendarEventModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void setEvents(const QList<CalendarEvent>& events);
  [[nodiscard]] const QList<CalendarEvent>& events() const { return events_; }

 private:
  QList<CalendarEvent> events_;
};
