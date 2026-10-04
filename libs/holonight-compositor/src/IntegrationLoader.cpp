#include "IntegrationLoader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

QStringList IntegrationLoader::defaultDirectories() {
  const QDir executable(QCoreApplication::applicationDirPath());
  return {
      executable.filePath(QStringLiteral("holonight/backends")),
      executable.filePath(QStringLiteral("../holonight/backends")),
      executable.filePath(QStringLiteral(HOLONIGHT_BACKEND_RELATIVE_PATH)),
  };
}
QList<IntegrationDescriptor> IntegrationLoader::discover(const QStringList& directories) {
  QList<IntegrationDescriptor> result;
  QSet<QString> paths;
  for (const QString& directory : directories) {
    const QDir dir(directory);
    for (const QString& file : dir.entryList(QDir::Files, QDir::Name)) {
      QString path = dir.filePath(file);
      QJsonObject metadata;
      if (file.endsWith(QStringLiteral(".json"))) {
        QFile catalog(path);
        if (!catalog.open(QIODevice::ReadOnly)) {
          continue;
        }
        metadata = QJsonDocument::fromJson(catalog.readAll()).object();
        if (metadata.value(QStringLiteral("iid")).toString() != QLatin1String(HolonightIntegration_iid)) {
          continue;
        }
        path = dir.filePath(metadata.value(QStringLiteral("library")).toString());
      } else {
        QPluginLoader candidate(path);
        const auto envelope = candidate.metaData();
        if (envelope.value(QStringLiteral("IID")).toString() != QLatin1String(HolonightIntegration_iid)) {
          continue;
        }
        metadata = envelope.value(QStringLiteral("MetaData")).toObject();
      }
      path = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
      if (paths.contains(path)) {
        continue;
      }
      paths.insert(path);
      result.append({.path = path, .metadata = metadata});
    }
  }
  return result;
}
QString IntegrationLoader::select(const QList<IntegrationDescriptor>& descriptors,
                                  const QProcessEnvironment& environment) {
  QSet<QString> desktops;
  QSet<QString> markers;
  QString fallback;
  for (const auto& descriptor : descriptors) {
    const auto& metadata = descriptor.metadata;
    const QString identifier = metadata.value(QStringLiteral("id")).toString();
    if (metadata.value(QStringLiteral("fallback")).toBool()) {
      fallback = identifier;
    }
    for (const auto token : metadata.value(QStringLiteral("desktops")).toArray()) {
      for (const auto& desktop : environment.value(QStringLiteral("XDG_CURRENT_DESKTOP")).split(':')) {
        if (desktop.trimmed().compare(token.toString(), Qt::CaseInsensitive) == 0) {
          desktops.insert(identifier);
        }
      }
    }
    for (const auto marker : metadata.value(QStringLiteral("markers")).toArray()) {
      if (!environment.value(marker.toString()).isEmpty()) {
        markers.insert(identifier);
      }
    }
  }
  if (!desktops.isEmpty()) {
    return desktops.size() == 1 ? *desktops.begin() : fallback;
  }
  return markers.size() == 1 ? *markers.begin() : fallback;
}
IntegrationLoader::IntegrationLoader(QObject* parent)
    : IntegrationLoader(defaultDirectories(), QProcessEnvironment::systemEnvironment(), parent) {}
IntegrationLoader::IntegrationLoader(const QStringList& directories, const QProcessEnvironment& environment,
                                     QObject* parent)
    : QObject(parent) {
  const auto descriptors = discover(directories);
  const auto selected = select(descriptors, environment);
  for (const auto& descriptor : descriptors) {
    if (descriptor.metadata.value(QStringLiteral("id")).toString() == selected && load(descriptor)) {
      return;
    }
  }
  if (diagnostic_.isEmpty()) {
    diagnostic_ = QStringLiteral("No selected integration plugin available");
  }
  qWarning().noquote() << diagnostic_;
  for (const auto& descriptor : descriptors) {
    if (descriptor.metadata.value(QStringLiteral("fallback")).toBool() && load(descriptor)) {
      return;
    }
  }
}
bool IntegrationLoader::load(const IntegrationDescriptor& descriptor) {
  if (descriptor.metadata.value(QStringLiteral("shellVersion")).toString() !=
      QLatin1String(HOLONIGHT_INTEGRATION_VERSION)) {
    diagnostic_ = QStringLiteral("Incompatible integration: %1").arg(descriptor.path);
    return false;
  }
  auto loader = std::make_unique<QPluginLoader>(descriptor.path);
  const auto embedded = loader->metaData();
  if (!embedded.isEmpty() &&
      (embedded.value(QStringLiteral("IID")).toString() != QLatin1String(HolonightIntegration_iid) ||
       embedded.value(QStringLiteral("MetaData")).toObject().value(QStringLiteral("shellVersion")).toString() !=
           QLatin1String(HOLONIGHT_INTEGRATION_VERSION))) {
    diagnostic_ = QStringLiteral("Incompatible integration binary: %1").arg(descriptor.path);
    return false;
  }
  auto* plugin = qobject_cast<IntegrationPlugin*>(loader->instance());
  if (plugin == nullptr) {
    diagnostic_ = QStringLiteral("Cannot load integration %1: %2").arg(descriptor.path, loader->errorString());
    return false;
  }
  integration_ = plugin;
  backend_name_ = descriptor.metadata.value(QStringLiteral("id")).toString();
  loader_ = std::move(loader);
  return true;
}
std::unique_ptr<CompositorBackend> IntegrationLoader::createCompositor() {
  if (integration_ == nullptr) {
    return {};
  }
  auto backend = integration_->createCompositor();
  contribution_model_ = integration_->topbarComponent().isEmpty() ? nullptr : backend.get();
  return backend;
}
