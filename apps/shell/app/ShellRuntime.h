#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
class ShellRuntime : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool troubleshootEnabled READ troubleshootEnabled CONSTANT)
 public:
  explicit ShellRuntime(QObject* parent = nullptr)
      : QObject(parent), troubleshoot_enabled_(qgetenv("HOLONIGHT_TROUBLESHOOT") == "true") {}
  bool troubleshootEnabled() const { return troubleshoot_enabled_; }

 private:
  const bool troubleshoot_enabled_;
};
