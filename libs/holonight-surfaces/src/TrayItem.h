#pragma once

#include <QDBusArgument>
#include <QImage>
#include <QString>

#include <cstdint>

// Raw icon pixel block as carried over D-Bus: (width, height, ARGB32 bytes)
struct SniIconPixel {
  int width{0};
  int height{0};
  QByteArray data;
};

using SniIconPixmapList = QList<SniIconPixel>;

QDBusArgument& operator<<(QDBusArgument& arg, const SniIconPixel& pix);
const QDBusArgument& operator>>(const QDBusArgument& arg, SniIconPixel& pix);

QDBusArgument& operator<<(QDBusArgument& arg, const SniIconPixmapList& list);
const QDBusArgument& operator>>(const QDBusArgument& arg, SniIconPixmapList& list);

// Untrusted D-Bus dimensions must never be multiplied at 32-bit width before validation —
// width*height*4 overflows a signed int well before reaching kMaxTrayPixmapDim^2.
constexpr int kMaxTrayPixmapDim{512};

enum class PixmapRejectReason : std::uint8_t {
  None,
  NonPositiveDimensions,
  DimensionTooLarge,
  DataLengthMismatch,
};

// Pure validation: no allocation, no I/O. Checks dimensions are positive and bounded
// before any multiplication, then verifies data_size is an exact match using 64-bit
// arithmetic so a 32-bit overflow can never produce a false "in bounds" result.
[[nodiscard]] PixmapRejectReason validateTrayPixmapDimensions(int width, int height, qsizetype data_size);

// Decode the best-fit pixmap from a list; returns 22×22 ARGB32 QImage.
// `service` is used only for diagnostic logging when a pixmap is rejected.
QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size = 22, const QString& service = {});

struct SniToolTip {
  QString icon_name;
  SniIconPixmapList icon_pixmap;
  QString title;
  QString description;
};

QDBusArgument& operator<<(QDBusArgument& arg, const SniToolTip& tip);
const QDBusArgument& operator>>(const QDBusArgument& arg, SniToolTip& tip);

struct TrayItem {
  QString service;
  QString object_path;
  QString id;
  QString icon_name;
  QString attention_icon_name;
  QString status;
  QString title;
  QImage icon_pixmap_cache;
  QImage clean_icon_pixmap_cache;
  QImage previous_clean_icon_pixmap_cache;
  QImage attention_pixmap_cache;
  SniToolTip tooltip;
  bool has_unread{false};

  [[nodiscard]] bool isPassive() const { return status == QLatin1String("Passive"); }
  [[nodiscard]] bool needsAttention() const { return status == QLatin1String("NeedsAttention"); }
};

// Register meta-types required for D-Bus argument streaming.
void registerTrayMetaTypes();
