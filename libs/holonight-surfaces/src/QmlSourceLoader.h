#pragma once

#include <QQmlError>
#include <QQuickView>
#include <QUrl>

inline bool loadQmlSource(QQuickView* view, const QUrl& source, const char* surface_name) {
  view->setSource(source);
  if (view->status() == QQuickView::Ready) {
    return true;
  }

  qCritical("%s: failed to load QML source %s", surface_name, qPrintable(source.toString()));
  for (const QQmlError& error : view->errors()) {
    qCritical("%s: %s", surface_name, qPrintable(error.toString()));
  }
  return false;
}
