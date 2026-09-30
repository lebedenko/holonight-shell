#pragma once
#include <QObject>
#include <QString>

class KeyboardLayoutProvider : public QObject {
  Q_OBJECT
 public:
  using QObject::QObject;
  virtual void start() = 0;
  [[nodiscard]] virtual QString layoutCode() const = 0;
  [[nodiscard]] virtual QString layoutName() const = 0;
 Q_SIGNALS:
  void layoutCodeChanged();
  void layoutNameChanged();
};
