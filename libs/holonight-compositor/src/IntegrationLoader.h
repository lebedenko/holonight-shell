#pragma once
#include "IntegrationPlugin.h"

#include <QJsonObject>
#include <QPluginLoader>
#include <QProcessEnvironment>
#include <QtQml/qqml.h>

struct IntegrationDescriptor {
  QString path;
  QJsonObject metadata;
};

class IntegrationLoader final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QUrl componentUrl READ componentUrl CONSTANT)
  Q_PROPERTY(QObject* contributionModel READ contributionModel CONSTANT)
 public:
  explicit IntegrationLoader(QObject* parent = nullptr);
  IntegrationLoader(const QStringList& directories, const QProcessEnvironment& environment, QObject* parent = nullptr);
  static QList<IntegrationDescriptor> discover(const QStringList& directories);
  static QString select(const QList<IntegrationDescriptor>& descriptors, const QProcessEnvironment& environment);
  static QStringList defaultDirectories();
  [[nodiscard]] IntegrationPlugin* integration() const { return integration_; }
  [[nodiscard]] QString backendName() const { return backend_name_; }
  [[nodiscard]] QString diagnostic() const { return diagnostic_; }
  std::unique_ptr<CompositorBackend> createCompositor();
  [[nodiscard]] QUrl componentUrl() const {
    return (integration_ != nullptr) ? integration_->topbarComponent() : QUrl{};
  }
  [[nodiscard]] QObject* contributionModel() const { return contribution_model_; }

 private:
  bool load(const IntegrationDescriptor& descriptor);
  std::unique_ptr<QPluginLoader> loader_;
  IntegrationPlugin* integration_{nullptr};
  QObject* contribution_model_{nullptr};
  QString backend_name_;
  QString diagnostic_;
};
