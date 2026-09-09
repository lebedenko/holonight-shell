#include "QuickControlsRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QSet>

Q_LOGGING_CATEGORY(lcControlsRuntime, "holonight.controls.runtime", QtWarningMsg)

namespace holonight {
void configureQuickControls() {
  const QString executable = QCoreApplication::applicationFilePath();
  const bool build = QFileInfo(executable).canonicalFilePath() ==
                     QFileInfo(QStringLiteral(HOLONIGHT_RUNTIME_BUILD_EXECUTABLE)).canonicalFilePath();
  const QString path = build ? QStringLiteral(HOLONIGHT_RUNTIME_QML_PATH)
                             : QDir(QCoreApplication::applicationDirPath())
                                   .absoluteFilePath(QStringLiteral(HOLONIGHT_RUNTIME_INSTALL_QML));
  QStringList paths = qEnvironmentVariable("QML_IMPORT_PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
  paths.append(QDir::cleanPath(path));
  paths.removeDuplicates();
  qputenv("QML_IMPORT_PATH", paths.join(QDir::listSeparator()).toUtf8());

  // Selection and directory availability are deliberately separate from loaded-control evidence.
  qCDebug(lcControlsRuntime) << "phase=selection configured=" << QQuickStyle::name();
  QQmlEngine discovery;
  qCDebug(lcControlsRuntime) << "phase=discovery roots=" << discovery.importPathList();
  for (const QString& module :
       {QStringLiteral("Holonight/Core"), QStringLiteral("Holonight/Controls"),
        QQuickStyle::name() == QStringLiteral("Holonight") ? QStringLiteral("Holonight") : QString{}}) {
    if (module.isEmpty()) {
      continue;
    }
    bool found = false;
    for (const QString& root : discovery.importPathList()) {
      if (QFileInfo::exists(root + QLatin1Char('/') + module + QStringLiteral("/qmldir"))) {
        found = true;
        break;
      }
    }
    if (!found) {
      qCWarning(lcControlsRuntime) << "phase=discovery classification=missing-module module=" << module
                                   << "roots=" << discovery.importPathList()
                                   << "Install the matching HoloNight provider or correct its QML prefix.";
    }
  }
}
void reportQuickControlsLoaded(QObject* root) {
  if (root == nullptr || !lcControlsRuntime().isDebugEnabled()) {
    return;
  }
  qCDebug(lcControlsRuntime) << "phase=ui classification=loaded" << root->metaObject()->className();
  auto objects = root->findChildren<QObject*>();
  objects.prepend(root);
  QSet<QString> origins;
  for (auto* object : objects) {
    const auto* context = qmlContext(object);
    if (context != nullptr) {
      const QString source = context->baseUrl().toString();
      for (const auto* meta = object->metaObject(); meta != nullptr; meta = meta->superClass()) {
        if (QByteArrayView(meta->className()) == QByteArrayView("QQuickLabel")) {
          qCDebug(lcControlsRuntime).noquote() << "phase=implementation instance=QQuickLabel context=" << source;
          break;
        }
      }
      if (!source.isEmpty() && !origins.contains(source)) {
        origins.insert(source);
        qCDebug(lcControlsRuntime).noquote() << "phase=implementation origin=" << source;
      }
    }
  }
}
}  // namespace holonight
