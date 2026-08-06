#include "FakeQmlServices.h"

#include <QDir>
#include <QFile>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

namespace {

class Setup : public QObject {
  Q_OBJECT
 public:
  Setup() {
    // 1. Create a temporary directory for our mock QML modules.
    if (!modules_dir_.isValid() || !QDir(modules_dir_.path()).mkpath(QStringLiteral("HolonightShell"))) {
      qFatal("Failed to create temporary directory for QML modules");
    }

    const QString source_root = QStringLiteral(TEST_SOURCE_DIR);
    const QString qmldir_path = modules_dir_.filePath(QStringLiteral("HolonightShell/qmldir"));
    if (!writeFile(qmldir_path, shellQmldir(source_root))) {
      qFatal("Failed to write mock qmldir to %s", qPrintable(qmldir_path));
    }

    // 2. Register global QML singletons using our FakeQmlServices instance.
    if (!services_.registerSingletons()) {
      qFatal("Failed to register FakeQmlServices singletons");
    }
  }

 public slots:
  void qmlEngineAvailable(QQmlEngine* engine) {
    // 3. Make sure each test QML engine can find the HolonightShell module.
    engine->addImportPath(modules_dir_.path());
  }

 private:
  QTemporaryDir modules_dir_;
  FakeQmlServices services_;
};

}  // namespace

QUICK_TEST_MAIN_WITH_SETUP(qml_harness, Setup)

#include "test_qml_harness.moc"
