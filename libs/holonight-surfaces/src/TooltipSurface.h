#pragma once

#include "TransientSurfaceHost.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

class QScreen;

class TooltipSurface : public TransientSurfaceHost {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QString title READ title NOTIFY contentChanged)
  Q_PROPERTY(QString description READ description NOTIFY contentChanged)
  Q_PROPERTY(QString iconName READ iconName NOTIFY contentChanged)
  Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY contentChanged)
  Q_PROPERTY(bool charging READ charging NOTIFY contentChanged)
  Q_PROPERTY(int signalStrength READ signalStrength NOTIFY contentChanged)
  Q_PROPERTY(bool tooltipVisible READ isTooltipVisible NOTIFY tooltipVisibleChanged)

 public:
  explicit TooltipSurface(QObject* parent = nullptr);
  ~TooltipSurface() override;

  TooltipSurface(const TooltipSurface&) = delete;
  TooltipSurface& operator=(const TooltipSurface&) = delete;
  TooltipSurface(TooltipSurface&&) = delete;
  TooltipSurface& operator=(TooltipSurface&&) = delete;

  [[nodiscard]] QString title() const { return title_; }
  [[nodiscard]] QString description() const { return description_; }
  [[nodiscard]] QString iconName() const { return icon_name_; }
  [[nodiscard]] int batteryPercent() const { return battery_percent_; }
  [[nodiscard]] bool charging() const { return charging_; }
  [[nodiscard]] int signalStrength() const { return signal_strength_; }
  [[nodiscard]] bool isTooltipVisible() const { return tooltip_visible_; }

  Q_INVOKABLE void show(const QString& screen_name, int anchor_x, int anchor_width, const QString& title,
                        const QString& description, const QString& icon_name, int battery_percent, bool charging,
                        int signal_strength);
  Q_INVOKABLE void hide();

  [[nodiscard]] static Holonight::Wayland::LayerSurfaceSpec surfaceSpec(QScreen* screen, int anchor_x,
                                                                        int anchor_width);

 Q_SIGNALS:
  void contentChanged();
  void tooltipVisibleChanged();

 private:
  [[nodiscard]] bool ensureSurface(const QString& screen_name, int anchor_x, int anchor_width);
  void destroySurface();
  void setTooltipVisible(bool visible);
  void setContent(const QString& title, const QString& description, const QString& icon_name, int battery_percent,
                  bool charging, int signal_strength);

  void onSurfaceTerminated() override;
  void onSurfaceConfigured() override;

  bool tooltip_visible_ = false;
  bool pending_show_ = false;
  QString pending_screen_;
  int pending_anchor_x_ = 0;
  int pending_anchor_width_ = 0;
  QString current_screen_;
  int current_anchor_x_ = 0;
  int current_anchor_width_ = 0;
  QString title_;
  QString description_;
  QString icon_name_;
  int battery_percent_ = 100;
  bool charging_ = false;
  int signal_strength_ = 100;
};
