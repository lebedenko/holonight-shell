#pragma once
#include <QAbstractListModel>
namespace Holonight::Authentication {
enum class MessageSeverity : quint8 { Information, Error };
struct AuthenticationMessage {
  MessageSeverity severity;
  QString text;
};
class MessageListModel final : public QAbstractListModel {
  Q_OBJECT
 public:
  // Qt item roles are integer constants consumed by QAbstractItemModel.
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size)
  enum Role : int { SeverityRole = Qt::UserRole + 1, TextRole };
  using QAbstractListModel::QAbstractListModel;
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  void setItems(QList<AuthenticationMessage> items);
  void append(AuthenticationMessage item);

 private:
  QList<AuthenticationMessage> items_;
};
}  // namespace Holonight::Authentication
