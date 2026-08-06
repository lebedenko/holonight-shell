#include "CalendarEventModel.h"

static constexpr int kDescriptionMaxChars{100};

CalendarEventModel::CalendarEventModel(QObject* parent) : QAbstractListModel(parent) {}

int CalendarEventModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(events_.size());
}

QVariant CalendarEventModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(events_.size())) {
    return {};
  }
  const auto& evt = events_.at(index.row());
  switch (role) {
    case EventIdRole:
      return evt.uid;
    case TitleRole:
      return evt.title;
    case StartTimeRole:
      return evt.start_time;
    case EndTimeRole:
      return evt.end_time;
    case IsAllDayRole:
      return evt.is_all_day;
    case ProviderRole:
      return evt.provider_type;
    case DescriptionRole:
      return evt.description.left(kDescriptionMaxChars);
    case LocationRole:
      return evt.location;
    case RruleRole:
      return evt.rrule;
    case DurationRole:
      return evt.duration;
    case AccessClassRole:
      return evt.access_class;
    case CreatedRole:
      return evt.created;
    case LastModifiedRole:
      return evt.last_modified;
    case OrganizerRole:
      return evt.organizer;
    case AttendeesRole:
      return evt.attendees;
    case CategoriesRole:
      return evt.categories;
    case StatusRole:
      return evt.status;
    case TransparencyRole:
      return evt.transparency;
    case UrlRole:
      return evt.url;
    case GeoRole:
      return evt.geo;
    case SequenceRole:
      return evt.sequence;
    case RecurrenceIdRole:
      return evt.recurrence_id;
    case ExdatesRole:
      return evt.exdates;
    case RdatesRole:
      return evt.rdates;
    case AttachmentsRole:
      return evt.attachments;
    case CommentsRole:
      return evt.comments;
    case ContactsRole:
      return evt.contacts;
    case RelatedToRole:
      return evt.related_to;
    case ResourcesRole:
      return evt.resources;
    case AlarmsRole:
      return evt.alarms;
    default:
      return {};
  }
}

QHash<int, QByteArray> CalendarEventModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {EventIdRole, "eventId"},
      {TitleRole, "title"},
      {StartTimeRole, "startTime"},
      {EndTimeRole, "endTime"},
      {IsAllDayRole, "isAllDay"},
      {ProviderRole, "provider"},
      {DescriptionRole, "description"},
      {LocationRole, "location"},
      {RruleRole, "rrule"},
      {DurationRole, "duration"},
      {AccessClassRole, "accessClass"},
      {CreatedRole, "created"},
      {LastModifiedRole, "lastModified"},
      {OrganizerRole, "organizer"},
      {AttendeesRole, "attendees"},
      {CategoriesRole, "categories"},
      {StatusRole, "status"},
      {TransparencyRole, "transparency"},
      {UrlRole, "url"},
      {GeoRole, "geo"},
      {SequenceRole, "sequence"},
      {RecurrenceIdRole, "recurrenceId"},
      {ExdatesRole, "exdates"},
      {RdatesRole, "rdates"},
      {AttachmentsRole, "attachments"},
      {CommentsRole, "comments"},
      {ContactsRole, "contacts"},
      {RelatedToRole, "relatedTo"},
      {ResourcesRole, "resources"},
      {AlarmsRole, "alarms"},
  };
  return kRoles;
}

void CalendarEventModel::setEvents(const QList<CalendarEvent>& events) {
  beginResetModel();
  events_ = events;
  endResetModel();
}
