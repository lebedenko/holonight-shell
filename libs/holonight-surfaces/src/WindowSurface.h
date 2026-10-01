#pragma once

#include "TransientSurfaceHost.h"

#include <QStringList>
#include <QtQml/qqml.h>

class WindowSurface final : public TransientSurfaceHost {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool visible READ visible NOTIFY changed)
  Q_PROPERTY(QString screenName READ screenName NOTIFY changed)
  Q_PROPERTY(int mode READ mode NOTIFY changed)
  Q_PROPERTY(QString target READ target NOTIFY changed)
  Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
 public:
  explicit WindowSurface(QObject* parent = nullptr) : TransientSurfaceHost("WindowSurface", parent) {}
  bool visible() const { return hasSurface(); }
  QString screenName() const { return screen_name_; }
  int mode() const { return mode_; }
  QString target() const { return target_; }
  QStringList choices() const { return choices_; }
  Q_INVOKABLE void toggle(const QString& screen = {});
  Q_INVOKABLE void menu(const QString& id, const QString& screen);
  Q_INVOKABLE void chooser(const QStringList& ids, const QString& screen);
  Q_INVOKABLE void desktopMenu(const QString& screen);
  Q_INVOKABLE void hide();
 Q_SIGNALS:
  void changed();
  void opened();
  void dismissed();

 private:
  void show(const QString& screen);
  void onSurfaceTerminated() override;
  int mode_{0};
  QString target_;
  QString screen_name_;
  QStringList choices_;
};
