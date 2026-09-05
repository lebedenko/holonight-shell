#pragma once

#include <QPointer>
#include <QQuickWindow>
#include <QScreen>
#include <QtQml/qqmlregistration.h>

class AuthenticationWindowGeometry : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged FINAL)

 public:
  explicit AuthenticationWindowGeometry(QObject* parent = nullptr) : QObject(parent) {}
  QQuickWindow* window() const { return window_; }
  void setWindow(QQuickWindow* window);
  Q_INVOKABLE void resetSize(int preferred_width, int preferred_height);

 signals:
  void windowChanged();

 private:
  void trackScreen();
  void reclamp();
  QPointer<QQuickWindow> window_;
  QPointer<QScreen> screen_;
};
