#include "FakeQmlServices.h"
#include "QuickControlsRuntime.h"

#include <QDir>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include <algorithm>

namespace {

#ifdef HOLONIGHT_COMPILED_ACCEPTANCE
// Icon lookup is an external desktop boundary; geometry tests need deterministic images.
class TestIconProvider : public QQuickImageProvider {
 public:
  TestIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  QImage requestImage(const QString& /*id*/, QSize* size, const QSize& requested) override {
    const QSize dimensions = requested.isValid() ? requested : QSize(32, 32);
    if (size != nullptr) {
      *size = dimensions;
    }
    QImage image(dimensions, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
  }
};
#endif

class Setup : public QObject {
  Q_OBJECT
 public:
  Setup() {
#ifndef HOLONIGHT_COMPILED_ACCEPTANCE
    // 1. Create a temporary directory for our mock QML modules.
    if (!modules_dir_.isValid() || !QDir(modules_dir_.path()).mkpath(QStringLiteral("HolonightShell"))) {
      qFatal("Failed to create temporary directory for QML modules");
    }

    const QString source_root = QStringLiteral(TEST_SOURCE_DIR);
    const QString qmldir_path = modules_dir_.filePath(QStringLiteral("HolonightShell/qmldir"));
    if (!writeFile(qmldir_path, shellQmldir(source_root))) {
      qFatal("Failed to write mock qmldir to %s", qPrintable(qmldir_path));
    }

#endif
  }

  // These methods are exposed through the QObject metaobject to QML.
  // NOLINTBEGIN(readability-convert-member-functions-to-static)
  [[nodiscard]] Q_INVOKABLE QString expectedStyle() const { return qEnvironmentVariable("UQC_EXPECTED_STYLE"); }
  [[nodiscard]] Q_INVOKABLE QString selectedStyle() const { return QQuickStyle::name(); }
  Q_INVOKABLE bool hasOrigin(QObject* object, const QString& suffix) const {
    auto objects = object->findChildren<QObject*>();
    objects.prepend(object);
    return std::ranges::any_of(objects, [&](QObject* child) {
      const auto* context = qmlContext(child);
      if (context != nullptr && context->baseUrl().toString().endsWith(suffix)) {
        qInfo().noquote() << "ORIGIN" << object->objectName() << context->baseUrl().toString();
        return true;
      }
      return false;
    });
  }
  // NOLINTEND(readability-convert-member-functions-to-static)

 public slots:
  void applicationAvailable() {
    holonight::configureQuickControls();
    services_ = std::make_unique<FakeQmlServices>();
    if (!services_->registerSingletons()) {
      qFatal("Failed to register FakeQmlServices singletons");
    }
  }
  void qmlEngineAvailable(QQmlEngine* engine) {
#ifdef HOLONIGHT_COMPILED_ACCEPTANCE
    engine->addImageProvider(QStringLiteral("icon"), new TestIconProvider());
#endif
    engine->rootContext()->setContextProperty(QStringLiteral("controlsEvidence"), this);
    // 3. Make sure each test QML engine can find the HolonightShell module.
#ifndef HOLONIGHT_COMPILED_ACCEPTANCE
    engine->addImportPath(modules_dir_.path());
#else
    engine->addImportPath(QStringLiteral("qrc:/"));
#endif
  }

 private:
  QTemporaryDir modules_dir_;
  std::unique_ptr<FakeQmlServices> services_;
};

}  // namespace

QUICK_TEST_MAIN_WITH_SETUP(qml_harness, Setup)

#include "test_qml_harness.moc"
