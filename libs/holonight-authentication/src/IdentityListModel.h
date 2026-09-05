#pragma once
#include <QAbstractListModel>
#include <QUrl>
#include <QVariantMap>
namespace Holonight::Authentication {
struct Identity {
  QString stable_id;
  QString display_label;
  uint uid{};
  bool has_uid{};
  QString username;
  QString full_name;
  QUrl avatar_url;
};
class IdentityListModel final : public QAbstractListModel {
  Q_OBJECT
 public:
  // Qt item roles are integer constants consumed by QAbstractItemModel.
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size)
  enum Role : int { StableIdRole = Qt::UserRole + 1, DisplayLabelRole, UsernameRole, FullNameRole, AvatarUrlRole };
  using QAbstractListModel::QAbstractListModel;
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  void setItems(QList<Identity> items);
  bool updateProfile(const Identity& profile);
  [[nodiscard]] const QList<Identity>& items() const { return items_; }
  [[nodiscard]] QVariantMap profile(const QString& stable_id) const;
  [[nodiscard]] bool contains(const QString& stable_id) const;
  [[nodiscard]] QString preferred(uint current_uid) const;

 private:
  QList<Identity> items_;
};
}  // namespace Holonight::Authentication
