#pragma once

#include "ConfigService.h"
#include "PerMonitorLayerManager.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

class LayerShell;
class LayerSurface;
class MonitorOccupancyService;
class QQuickItem;
class QQuickView;
class QScreen;

// Manages one desktop widget definition across the monitors it targets: a wlr-layer-shell surface on
// the `bottom` layer per (monitor, widget), shown only while that monitor's visible workspace is empty
// (gated by MonitorOccupancyService) and hidden — but kept alive — otherwise. One WidgetManager is
// created per WidgetDefinition; the set of managers is rebuilt on config reload. A single shared
// QTimer drives the countdown for all of this widget's surfaces; it is frozen while every surface is
// hidden so occluded widgets do no work.
class WidgetManager : public PerMonitorLayerManager {
  Q_OBJECT

 public:
  WidgetManager(LayerShell& shell, WidgetDefinition definition, int margin, int index,
                QList<QStringList> position_blockers, MonitorOccupancyService* occupancy, QObject* parent = nullptr);

 protected:
  [[nodiscard]] LayerConfig layerConfig() const override;
  void configureSurface(LayerSurface& surface, QScreen* screen) override;
  [[nodiscard]] QmlSource qmlSource(QScreen* screen) override;
  [[nodiscard]] bool shouldCreateSurface(QScreen* screen) const override;

 private Q_SLOTS:
  void onOccupancyChanged(const QString& monitor_name, bool is_empty);
  void onTick();

 private:
  // Show/hide the surface for one monitor to match current occupancy, then reconcile the timer.
  void applyVisibility(const QString& monitor_name);
  // Run the timer iff at least one surface is visible; resync the countdown when (re)starting.
  void updateTimerState();
  // Recompute this widget's strings and push them into every live surface (type-dispatched).
  void recomputeAndPropagate();
  void startTickTimer();
  [[nodiscard]] bool anySurfaceVisible() const;
  [[nodiscard]] QString currentCountdownText() const;
  [[nodiscard]] QString deadlineLabelText() const;
  // True once the time-to-event deadline has passed; always false for the clock (which never ends).
  [[nodiscard]] bool isPastDeadline() const;
  // Recompute the clock's time/seconds/date strings from the current wall-clock into the cache fields.
  void recomputeClockStrings();
  // Push the cached clock strings into one surface's QML root.
  void pushClockStrings(QQuickItem* root) const;
  // Short human label for collision/error logging (TTE title, or "clock@<position>").
  [[nodiscard]] QString widgetLabel() const;

  WidgetDefinition definition_;
  int margin_;
  int index_;
  // Monitor filters of earlier widgets sharing this widget's position; an entry matches a monitor when
  // it is empty (that widget targets all monitors) or contains the monitor name.
  QList<QStringList> position_blockers_;
  MonitorOccupancyService* occupancy_;
  QTimer tick_timer_;
  QString remaining_text_;  // time-to-event countdown string
  // Clock widget cache: the large HH:mm, the small seconds, and the date row, pushed to QML each tick.
  QString clock_time_text_;
  QString clock_seconds_text_;
  QString clock_date_text_;
  bool date_format_warned_{false};  // warn-once: custom date_format produced no output
  bool locale_warned_{false};       // warn-once: configured locale was unrecognized
  // Desired content visibility per monitor. The layer surface stays mapped across occupancy changes
  // (Qt's QWindow hide()/show() breaks the manually-bound wlr layer-shell role on remap); we instead
  // toggle the QML root's visibility, which keeps the QQuickView/QML cached and the hidden surface
  // transparent and input-transparent.
  QHash<QString, bool> content_visible_;
  mutable QSet<QString> warned_collisions_;  // monitors already warned about (warn-once, REQ-F-016)
};
