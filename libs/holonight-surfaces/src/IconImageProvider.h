#pragma once

#include <QFile>
#include <QIcon>
#include <QLoggingCategory>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>
#include <QStandardPaths>
#include <QUrl>

class IconImageProvider : public QQuickImageProvider {
 public:
  IconImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

  QPixmap requestPixmap(const QString& iconName, QSize* size, const QSize& requestedSize) override {
    const QSize resolved = requestedSize.isValid() ? requestedSize : QSize(16, 16);
    if (size != nullptr) {
      *size = resolved;
    }
    QIcon icon;
    if (iconName.startsWith(u'/')) {
      icon = QIcon(iconName);
    } else if (iconName.startsWith(QLatin1String("file://"))) {
      icon = QIcon(QUrl(iconName).toLocalFile());
    } else {
      icon = QIcon::fromTheme(iconName);
      if (icon.isNull()) {
        icon = iconFromPixmapDirs(iconName);
      }
      if (icon.isNull()) {
        static const QLoggingCategory kCat("holonight.icon_provider");
        qCDebug(kCat, "icon '%s' not found in theme or pixmaps dirs, using application-x-executable",
                qPrintable(iconName));
        icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
      }
    }
    if (icon.isNull()) {
      return {};
    }
    QPixmap pixmap = icon.pixmap(resolved);
    if (pixmap.isNull()) {
      qWarning("IconImageProvider: pixmap unavailable for '%s' at %dx%d", qPrintable(iconName), resolved.width(),
               resolved.height());
    }
    return pixmap;
  }

 private:
  static QIcon iconFromPixmapDirs(const QString& iconName) {
    static const QStringList kExtensions = {
        QStringLiteral(".png"),
        QStringLiteral(".svg"),
        QStringLiteral(".xpm"),
    };
    static const QStringList kDirs = pixmapDirs();

    for (const QString& dir : kDirs) {
      for (const QString& ext : kExtensions) {
        const QString path = dir + u'/' + iconName + ext;
        if (QFile::exists(path)) {
          return QIcon(path);
        }
      }
    }
    return {};
  }

  static QStringList pixmapDirs() {
    QStringList dirs;
    // User-local first, then system-wide.
    const QString local = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!local.isEmpty()) {
      dirs.append(local + QStringLiteral("/pixmaps"));
    }
    dirs.append(QStringLiteral("/usr/share/pixmaps"));
    return dirs;
  }
};
