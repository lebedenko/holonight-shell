#include "StatusPopupSurface.h"

#include "IconImageProvider.h"
#include "ShellConstants.h"
#include "StatusPopupGeometry.h"

#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <QThread>

#include <atomic>
#include <memory>

using namespace Holonight::Wayland;

namespace {
bool popupTraceEnabled() {
  static const bool enabled = qEnvironmentVariable("HOLONIGHT_STATUS_POPUP_TRACE") == "1";
  return enabled;
}

// Called only on the GUI thread. Never serialize model values or arbitrary object names.
void tracePopup(const char* event, QQuickView* view, quint64 frame = 0, qint64 emitted_ms = 0) {
  if (!popupTraceEnabled() || view == nullptr) {
    return;
  }
  static QFile output;
  if (output.fileName().isEmpty()) {
    QDir().mkpath("build/popup-resize-trace");
    output.setFileName(
        QStringLiteral("build/popup-resize-trace/geometry-%1.jsonl").arg(QCoreApplication::applicationPid()));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Append)) {
      qWarning("Cannot open popup geometry trace");
      return;
    }
  }
  if (!output.isOpen()) {
    return;
  }
  QJsonObject record{{"event", event},
                     {"utc_ms", QDateTime::currentMSecsSinceEpoch()},
                     {"frame", static_cast<qint64>(frame)},
                     {"emitted_utc_ms", emitted_ms},
                     {"snapshot_phase", emitted_ms == 0 ? "event" : "swap-delivery"},
                     {"actual_width", view->width()},
                     {"actual_height", view->height()},
                     {"surface_x", view->x()},
                     {"surface_y", view->y()},
                     {"scale", view->devicePixelRatio()},
                     {"surface_instance", QString::number(reinterpret_cast<quintptr>(view), 16)},
                     {"pointer_x", QCursor::pos().x()},
                     {"pointer_y", QCursor::pos().y()}};
  auto* root = qobject_cast<QQuickItem*>(view->rootObject());
  if (root != nullptr) {
    record.insert("root_width", root->width());
    record.insert("root_height", root->height());
    record.insert("frame_x", QJsonValue::fromVariant(root->property("frameX")));
    record.insert("frame_width", QJsonValue::fromVariant(root->property("frameWidth")));
    record.insert("frame_height", QJsonValue::fromVariant(root->property("frameHeight")));
    record.insert("requested_width", root->implicitWidth());
    record.insert("requested_height", root->implicitHeight());
    QJsonArray items;
    const auto visit = [&](const auto& self, QQuickItem* item) -> void {
      const QString name = item->objectName();
      if (item == root || name == "statusPopupContentLoader" || name == "audioCurrentDeviceRow" ||
          name == "popupTraceLayout" || name == "popupTraceHeader" || name == "popupTraceHero" ||
          name == "popupTraceBody" || name == "popupTraceFooter" || name == "audioMasterPanel" ||
          name == "popupContentViewport" || name == "audioPopupViewport" || name == "audioPopupPinnedHeader" ||
          name == "outputDeviceSection" || name == "inputDeviceSection" || name == "section-Connected" ||
          name == "section-Paired" || name == "section-Available" || name == "devices-Connected" ||
          name == "devices-Paired" || name == "devices-Available") {
        const QPointF origin = item->mapToItem(root, QPointF());
        QJsonObject bounds{{"role", name},
                           {"instance", QString::number(reinterpret_cast<quintptr>(item), 16)},
                           {"parent_instance", QString::number(reinterpret_cast<quintptr>(item->parentItem()), 16)},
                           {"x", origin.x()},
                           {"y", origin.y()},
                           {"width", item->width()},
                           {"height", item->height()},
                           {"implicit_height", item->implicitHeight()},
                           {"visible", item->isVisible()}};
        for (const char* property :
             {"contentY", "contentHeight", "pinned", "outputExpanded", "inputExpanded", "expanded"}) {
          const QVariant value = item->property(property);
          if (value.isValid()) {
            bounds.insert(QString::fromLatin1(property), QJsonValue::fromVariant(value));
          }
        }
        items.append(bounds);
      }
      for (QQuickItem* child : item->childItems()) {
        self(self, child);
      }
    };
    visit(visit, root);
    record.insert("items", items);
  }
  output.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
  output.flush();
}
}  // namespace

StatusPopupSurface::StatusPopupSurface(QObject* parent) : PairedTransientSurfaceHost("StatusPopupSurface", parent) {}
StatusPopupSurface::~StatusPopupSurface() { destroySurface(); }

void StatusPopupSurface::toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width) {
  if (popup_visible_ && active_popup_id_ == popup_id) {
    hide();
    return;
  }
  show(popup_id, screen_name, anchor_x, anchor_width);
}

void StatusPopupSurface::show(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width) {
  destroySurface();
  setPopupVisible(ensureSurface(popup_id, screen_name, anchor_x, anchor_width));
}

void StatusPopupSurface::hide() {
  destroySurface();
  setPopupVisible(false);
  setActivePopupId({});
}

bool StatusPopupSurface::ensureSurface(const QString& popup_id, const QString& screen_name, int anchor_x,
                                       int anchor_width) {
  QScreen* screen = QGuiApplication::primaryScreen();
  if (!screen_name.isEmpty()) {
    for (QScreen* candidate : QGuiApplication::screens()) {
      if (candidate->name() == screen_name) {
        screen = candidate;
        break;
      }
    }
  }
  if (screen == nullptr) {
    qCritical("StatusPopupSurface: no screen available");
    return false;
  }

  current_screen_ = screen;
  anchor_local_x_ = anchor_x - screen->geometry().x();
  anchor_width_ = anchor_width;
  applied_placement_ =
      statusPopupSurfacePlacement(screen->geometry(), screen->availableGeometry(), anchor_x, anchor_width);
  setActivePopupId(popup_id);
  return openPair(surfaceSpec(screen, popup_id, anchor_x, anchor_width));
}

PairedLayerSurfaceSpec StatusPopupSurface::surfaceSpec(QScreen* screen, const QString& popup_id, int anchor_x,
                                                       int anchor_width) {
  const StatusPopupSurfacePlacement placement =
      statusPopupSurfacePlacement(screen->geometry(), screen->availableGeometry(), anchor_x, anchor_width);
  return {
      .dismiss =
          {
              .output = screen,
              .name_space = QStringLiteral("status-popup-dismiss"),
              .layer = Layer::Top,
              .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
              .width = 0,
              .height = 0,
              .exclusive_zone = 0,
              .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopupDismissOverlay.qml")),
              .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
              .color = Qt::transparent,
          },
      .content =
          {
              .output = screen,
              .name_space = QStringLiteral("status-popup"),
              .layer = Layer::Top,
              .anchors = Anchor::Top | Anchor::Left,
              .width = placement.surface_size.width(),
              .height = placement.surface_size.height(),
              .margin_top = kStatusPopupTopGap,
              .margin_left = placement.left_margin,
              .exclusive_zone = 0,
              .keyboard_interactivity = KeyboardInteractivity::OnDemand,
              // Clicks pass through until QML publishes the panel bounds (onPairOpened).
              .input_region_policy = InputRegionPolicy::Empty,
              .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopup.qml")),
              .initial_properties = {{QStringLiteral("popupId"), popup_id},
                                     {QStringLiteral("maximumSurfaceHeight"), placement.surface_size.height()},
                                     {QStringLiteral("anchorCenter"), placement.anchor_center}},
              .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
              .color = Qt::transparent,
              .before_load =
                  [](QQmlEngine* engine) { engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider()); },
          },
  };
}

void StatusPopupSurface::onPairOpened() {
  applied_input_rect_.reset();
  if (popupTraceEnabled()) {
    auto* view = contentView();
    tracePopup("open", view);
    const auto frame = std::make_shared<std::atomic<quint64>>(0);
    const auto synchronized_frame = std::make_shared<std::atomic<quint64>>(0);
    connect(view, &QQuickWindow::afterAnimating, view,
            [view, frame] { tracePopup("gui-snapshot-before-polish", view, ++(*frame)); });
    connect(
        view, &QQuickWindow::beforeSynchronizing, view,
        [view, frame, synchronized_frame] {
          synchronized_frame->store(frame->load());
          // The basic loop synchronizes on the GUI thread after layout polish.
          // Never traverse live QML items from the threaded render loop.
          if (QThread::currentThread() == view->thread()) {
            tracePopup("synchronized-before-render", view, frame->load());
          }
        },
        Qt::DirectConnection);
    // Render callbacks capture scalar markers only. Geometry is read on the GUI thread.
    connect(
        view, &QQuickWindow::frameSwapped, view,
        [view, synchronized_frame] {
          const auto number = synchronized_frame->load();
          const auto timestamp = QDateTime::currentMSecsSinceEpoch();
          QMetaObject::invokeMethod(
              view,
              [view, number, timestamp] {
                // Geometry here is delivery-time state, not render-thread state.
                tracePopup("frame-swapped", view, number, timestamp);
              },
              Qt::QueuedConnection);
        },
        Qt::DirectConnection);
    connect(contentHost(), &LayerSurfaceHost::configured, view, [view] { tracePopup("configure", view); });
  }
  setPopupVisible(true);
  auto* root = contentRootObject();
  // The panel moves inside the fixed surface, so only the input region needs C++ follow-up. It has
  // to track the binding: panel changes land during polish, with no later frame guaranteed.
  if (root != nullptr) {
    QQmlProperty(root, QStringLiteral("panelBounds")).connectNotifySignal(this, SLOT(updateInputRegion()));
  }
  updateInputRegion();
  screen_connection_ =
      connect(current_screen_, &QScreen::geometryChanged, this, &StatusPopupSurface::updateSurfacePlacement);
  available_connection_ =
      connect(current_screen_, &QScreen::availableGeometryChanged, this, &StatusPopupSurface::updateSurfacePlacement);
}

void StatusPopupSurface::onPairConfigured() {
  // Either surface of the pair can report first; the entrance waits for the visible one.
  auto* root = contentRootObject();
  if (root != nullptr && contentHost() != nullptr && contentHost()->state() == LayerSurfaceHost::State::Configured) {
    root->setProperty("geometryReady", true);
  }
}

void StatusPopupSurface::updateInputRegion() {
  auto* root = contentRootObject();
  if (root == nullptr || contentHost() == nullptr) {
    return;
  }
  const QRectF bounds = root->property("panelBounds").toRectF();
  const QRect frame = bounds.toAlignedRect();
  const QRect input_rect = statusPopupInputRect(frame.size()).translated(frame.topLeft());
  if (applied_input_rect_ != input_rect &&
      contentHost()->setInputRegion(input_rect.isEmpty() ? InputRegionPolicy::Empty : InputRegionPolicy::Region,
                                    QRegion(input_rect))) {
    applied_input_rect_ = input_rect;
  }
}

// Screen changes are the only remaining resize; content changes never reach this path.
void StatusPopupSurface::updateSurfacePlacement() {
  auto* root = contentRootObject();
  if (root == nullptr || current_screen_ == nullptr || contentHost() == nullptr) {
    return;
  }
  const auto placement = statusPopupSurfacePlacement(current_screen_->geometry(), current_screen_->availableGeometry(),
                                                     current_screen_->geometry().x() + anchor_local_x_, anchor_width_);
  if (placement == applied_placement_) {
    return;
  }
  applied_placement_ = placement;
  root->setProperty("maximumSurfaceHeight", placement.surface_size.height());
  root->setProperty("anchorCenter", placement.anchor_center);
  contentHost()->setSize(placement.surface_size.width(), placement.surface_size.height());
  contentHost()->setMargins(kStatusPopupTopGap, 0, 0, placement.left_margin);
}

void StatusPopupSurface::onPairTerminated() {
  disconnect(screen_connection_);
  disconnect(available_connection_);
  setPopupVisible(false);
  if (!hasPendingPair()) {
    setActivePopupId({});
  }
}

void StatusPopupSurface::destroySurface() {
  tracePopup("close", contentView());
  disconnect(screen_connection_);
  disconnect(available_connection_);
  clearPendingPair();
  closePair();
}

void StatusPopupSurface::setPopupVisible(bool visible) {
  if (popup_visible_ == visible) {
    return;
  }
  popup_visible_ = visible;
  Q_EMIT popupVisibleChanged();
}

void StatusPopupSurface::setActivePopupId(const QString& popup_id) {
  if (active_popup_id_ == popup_id) {
    return;
  }
  active_popup_id_ = popup_id;
  Q_EMIT activePopupChanged();
}
