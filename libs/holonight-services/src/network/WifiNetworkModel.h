#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include <optional>

struct WifiNetwork {
  QString ssid;
  int strength{0};
  bool secured{false};
  bool known{false};
  bool connected{false};
  bool active{false};
  QString access_point_path;
  QString device_path;
  QString connection_path;
  uint frequency{0};
  QString status_text;
};

[[nodiscard]] bool operator==(const WifiNetwork& left, const WifiNetwork& right);

class WifiNetworkModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum class Role : uint16_t {
    Ssid = Qt::UserRole + 1,
    Strength,
    Secured,
    Known,
    Connected,
    Active,
    AccessPointPath,
    DevicePath,
    ConnectionPath,
    Frequency,
    StatusText,
  };

  explicit WifiNetworkModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] std::optional<WifiNetwork> networkAt(int row) const;
  void setNetworks(QList<WifiNetwork> networks);
  void clear();

 private:
  QList<WifiNetwork> networks_;
};
