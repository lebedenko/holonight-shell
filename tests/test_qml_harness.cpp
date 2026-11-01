#include "AuthenticationUiFixture.h"
#include "FakeQmlServices.h"
#include "IconImageProvider.h"
#include "QuickControlsRuntime.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QThread>
#include <QtMath>
#include <QtQuickTest/quicktest.h>

#include <algorithm>

namespace {

#ifdef HOLONIGHT_COMPILED_ACCEPTANCE
// Icon lookup is an external desktop boundary; geometry tests need deterministic images.
class TestIconProvider : public QQuickImageProvider {
 public:
  TestIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  QImage requestImage(const QString& identifier, QSize* size, const QSize& requested) override {
    const QSize dimensions = requested.isValid() ? requested : QSize(32, 32);
    if (size != nullptr) {
      *size = dimensions;
    }
    if (identifier.startsWith("strict/")) {
      return QImage(identifier.mid(7));
    }
    QImage image(dimensions, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
  }
};
#endif

#ifndef HOLONIGHT_COMPILED_ACCEPTANCE
class ShellSourceInterceptor : public QQmlAbstractUrlInterceptor {
 public:
  QUrl intercept(const QUrl& url, DataType) override {
    const QString prefix = QStringLiteral("qrc:/HolonightShell/");
    if (url.toString().startsWith(prefix) && url.path().endsWith(".qml")) {
      return QUrl::fromLocalFile(QStringLiteral(TEST_SOURCE_DIR) + "/apps/shell/qml/" +
                                 url.toString().mid(prefix.size()));
    }
    return url;
  }
};
#endif

class Setup : public QObject {
  Q_OBJECT
 public:
  Setup() {
    QImage icon(15, 15, QImage::Format_ARGB32);
    icon.fill(QColor("#c14287"));
    if (!icon.save(modules_dir_.filePath("window-icon.png"))) {
      qFatal("Cannot create test icon");
    }
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
  Q_INVOKABLE [[nodiscard]] QString coloredIconPath() const { return modules_dir_.filePath("window-icon.png"); }
  Q_INVOKABLE QObject* captureItem(QQuickItem* item) {
    if (item == nullptr || item->window() == nullptr) {
      return nullptr;
    }
    const auto bounds = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    const qreal ratio = item->window()->devicePixelRatio();
    const QRect pixels(qFloor(bounds.x() * ratio), qFloor(bounds.y() * ratio), qCeil(bounds.width() * ratio),
                       qCeil(bounds.height() * ratio));
    // Reuse the image wrapper used by authentication rendering tests. QtTest's
    // grabImage crops in logical coordinates, which truncates fractional-DPR captures.
    return new AuthenticationImage(item->window()->grabWindow().copy(pixels), this);
  }
  Q_INVOKABLE bool beginPopupFrames(QQuickItem* root) {
    disconnect(popup_frames_connection_);
    popup_frames_.clear();
    if (qEnvironmentVariable("QSG_RENDER_LOOP") != "basic" || !root || !root->window()) {
      return false;
    }
    popup_frames_connection_ = connect(
        root->window(), &QQuickWindow::beforeSynchronizing, root,
        [this, root] {
          Q_ASSERT(QThread::currentThread() == thread());
          QVariantMap frame{{"allocation", QVariantMap{{"width", root->width()},
                                                       {"height", root->height()},
                                                       {"requestedHeight", root->implicitHeight()}}}};
          auto items = root->findChildren<QQuickItem*>();
          items.prepend(root);
          for (auto* item : items) {
            const auto name = item->objectName();
            if (name == "audioMasterPanel" || name == "outputDeviceSection" || name == "inputDeviceSection" ||
                name == "audioCurrentDeviceRow" || name == "audioPopupPinnedHeader" || name == "popupTraceFooter" ||
                name == "audioPopupViewport" || name == "masterVolumeBadge" || name == "masterVolumeSlider" ||
                name == "masterVolumePercentageText" || name == "statusPopupContentLoader") {
              const auto position = item->mapToItem(root, QPointF{});
              const auto key = name + (name == "audioCurrentDeviceRow" ? item->parentItem()->objectName() : QString{});
              frame.insert(
                  key, QVariantMap{{"y", position.y()},
                                   {"height", item->height()},
                                   {"width", item->width()},
                                   {"x", position.x()},
                                   {"contentY", item->property("contentY")},
                                   {"expanded", item->property("expanded")},
                                   {"instance", QString::number(reinterpret_cast<quintptr>(item), 16)},
                                   {"parent", QString::number(reinterpret_cast<quintptr>(item->parentItem()), 16)}});
            }
          }
          popup_frames_.append(frame);
        },
        Qt::DirectConnection);
    return true;
  }
  Q_INVOKABLE QVariantList endPopupFrames() {
    disconnect(popup_frames_connection_);
    if (!popup_frames_.isEmpty() && qEnvironmentVariable("HOLONIGHT_STATUS_POPUP_TRACE") == "1") {
      QDir().mkpath("build/popup-resize-trace");
      QFile output(
          QStringLiteral("build/popup-resize-trace/test-frames-%1.jsonl").arg(QCoreApplication::applicationPid()));
      if (output.open(QIODevice::WriteOnly | QIODevice::Append)) {
        output.write(QJsonDocument::fromVariant(popup_frames_).toJson(QJsonDocument::Compact) + '\n');
      }
    }
    const auto frames = popup_frames_;
    popup_frames_.clear();
    return frames;
  }
  // NOLINTEND(readability-convert-member-functions-to-static)

 public slots:
  void applicationAvailable() {
    holonight::configureQuickControls();
    qmlRegisterType<AuthenticationUiFixture>("Holonight.Authentication.Test", 1, 0, "AuthenticationUiFixture");
    services_ = std::make_unique<FakeQmlServices>();
    if (!services_->registerSingletons()) {
      qFatal("Failed to register FakeQmlServices singletons");
    }
  }
  void qmlEngineAvailable(QQmlEngine* engine) {
    engine->addImportPath(QStringLiteral("qrc:/"));
#ifdef HOLONIGHT_COMPILED_ACCEPTANCE
    engine->addImageProvider(QStringLiteral("icon"), new TestIconProvider());
#else
    engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
#endif
    engine->rootContext()->setContextProperty(QStringLiteral("controlsEvidence"), this);
    // 3. Make sure each test QML engine can find the HolonightShell module.
#ifndef HOLONIGHT_COMPILED_ACCEPTANCE
    engine->addImportPath(modules_dir_.path());
    engine->addUrlInterceptor(&source_interceptor_);
#endif
  }

 private:
#ifndef HOLONIGHT_COMPILED_ACCEPTANCE
  ShellSourceInterceptor source_interceptor_;
#endif
  QMetaObject::Connection popup_frames_connection_;
  QVariantList popup_frames_;
  QTemporaryDir modules_dir_;
  std::unique_ptr<FakeQmlServices> services_;
};

}  // namespace

QUICK_TEST_MAIN_WITH_SETUP(qml_harness, Setup)

#include "test_qml_harness.moc"
