#include "TrayItem.h"

#include <QDBusMetaType>
#include <QLoggingCategory>
#include <QMetaType>

Q_LOGGING_CATEGORY(lcTrayItem, "holonight.tray.item")

QDBusArgument& operator<<(QDBusArgument& arg, const SniIconPixel& pix) {
  arg.beginStructure();
  arg << pix.width << pix.height << pix.data;
  arg.endStructure();
  return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, SniIconPixel& pix) {
  arg.beginStructure();
  arg >> pix.width >> pix.height >> pix.data;
  arg.endStructure();
  return arg;
}

QDBusArgument& operator<<(QDBusArgument& arg, const SniIconPixmapList& list) {
  arg.beginArray(qMetaTypeId<SniIconPixel>());
  for (const auto& pix : list) {
    arg << pix;
  }
  arg.endArray();
  return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, SniIconPixmapList& list) {
  list.clear();
  arg.beginArray();
  while (!arg.atEnd()) {
    SniIconPixel pix;
    arg >> pix;
    list.append(pix);
  }
  arg.endArray();
  return arg;
}

PixmapRejectReason validateTrayPixmapDimensions(int width, int height, qsizetype data_size) {
  if (width <= 0 || height <= 0) {
    return PixmapRejectReason::NonPositiveDimensions;
  }
  if (width > kMaxTrayPixmapDim || height > kMaxTrayPixmapDim) {
    return PixmapRejectReason::DimensionTooLarge;
  }
  const auto expected_size = static_cast<qint64>(width) * static_cast<qint64>(height) * 4;
  if (static_cast<qint64>(data_size) != expected_size) {
    return PixmapRejectReason::DataLengthMismatch;
  }
  return PixmapRejectReason::None;
}

QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size, const QString& service) {
  if (pixmaps.isEmpty()) {
    qCInfo(lcTrayItem) << "IconPixmap decode skipped: empty pixmap list";
    return {};
  }

  // Pick the pixmap whose size is closest to target_size (prefer larger).
  const SniIconPixel* best = pixmaps.constData();
  const bool info_logging_enabled = lcTrayItem().isInfoEnabled();
  QStringList candidates;
  for (const auto& pix : pixmaps) {
    if (info_logging_enabled) {
      candidates.append(QStringLiteral("%1x%2/%3b").arg(pix.width).arg(pix.height).arg(pix.data.size()));
    }
    if (pix.width == 0 || pix.height == 0) {
      continue;
    }
    int cur_diff = qAbs(best->width - target_size);
    int new_diff = qAbs(pix.width - target_size);
    if (new_diff < cur_diff || (new_diff == cur_diff && pix.width > best->width)) {
      best = &pix;
    }
  }
  if (info_logging_enabled) {
    qCInfo(lcTrayItem) << "IconPixmap candidates" << candidates << "target" << target_size << "selected"
                       << QSize(best->width, best->height) << "bytes" << best->data.size();
  }

  const PixmapRejectReason reject_reason = validateTrayPixmapDimensions(best->width, best->height, best->data.size());
  if (reject_reason != PixmapRejectReason::None) {
    qCWarning(lcTrayItem) << "IconPixmap decode rejected for service" << service << "reason"
                          << static_cast<int>(reject_reason) << "dimensions" << QSize(best->width, best->height)
                          << "data bytes" << best->data.size();
    return {};
  }

  // Wire format is big-endian ARGB32: bytes [A, R, G, B] per pixel.
  // QImage::Format_ARGB32 stores QRgb values, whose channels qRgba supplies.
  QImage img(best->width, best->height, QImage::Format_ARGB32);
  const QByteArray& raw = best->data;
  for (int row = 0; row < best->height; ++row) {
    for (int column = 0; column < best->width; ++column) {
      const int offset = ((row * best->width) + column) * 4;
      img.setPixel(column, row,
                   qRgba(static_cast<uchar>(raw.at(offset + 1)),  // R
                         static_cast<uchar>(raw.at(offset + 2)),  // G
                         static_cast<uchar>(raw.at(offset + 3)),  // B
                         static_cast<uchar>(raw.at(offset))));    // A
    }
  }

  if (img.width() == target_size && img.height() == target_size) {
    qCInfo(lcTrayItem) << "IconPixmap decoded without scaling" << img.size();
    return img;
  }
  QImage scaled = img.scaled(target_size, target_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  qCInfo(lcTrayItem) << "IconPixmap decoded and scaled" << img.size() << "->" << scaled.size();
  return scaled;
}

QDBusArgument& operator<<(QDBusArgument& arg, const SniToolTip& tip) {
  arg.beginStructure();
  arg << tip.icon_name << tip.icon_pixmap << tip.title << tip.description;
  arg.endStructure();
  return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, SniToolTip& tip) {
  arg.beginStructure();
  arg >> tip.icon_name >> tip.icon_pixmap >> tip.title >> tip.description;
  arg.endStructure();
  return arg;
}

void registerTrayMetaTypes() {
  qDBusRegisterMetaType<SniIconPixel>();
  qDBusRegisterMetaType<SniIconPixmapList>();
  qDBusRegisterMetaType<SniToolTip>();
}
