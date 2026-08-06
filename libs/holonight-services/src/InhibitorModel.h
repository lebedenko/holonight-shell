#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include <cstdint>

struct InhibitorEntry {
  QString who;
  QString why;

  bool operator==(const InhibitorEntry& other) const { return who == other.who && why == other.why; }
};

// QAbstractListModel wrapping active sleep inhibitors from logind ListInhibitors().
// setEntries() replaces the list atomically. Exposed from SuspendInhibitorService.inhibitorModel.
class InhibitorModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

 public:
  enum Roles : std::uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class)
    WhoRole = Qt::UserRole + 1,
    WhyRole,
  };
  Q_ENUM(Roles)

  explicit InhibitorModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] int count() const { return static_cast<int>(entries_.size()); }
  void setEntries(const QList<InhibitorEntry>& entries);

 Q_SIGNALS:
  void countChanged();

 private:
  QList<InhibitorEntry> entries_;
};
