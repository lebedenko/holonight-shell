#pragma once
#include "KeyboardLayoutProvider.h"

#include <QtQml/qqml.h>

#include <memory>

class KeyboardLayoutService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool available READ available NOTIFY layoutCodeChanged)
  Q_PROPERTY(QString layoutCode READ layoutCode NOTIFY layoutCodeChanged)
  Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutNameChanged)
 public:
  explicit KeyboardLayoutService(QObject* parent = nullptr);
  explicit KeyboardLayoutService(std::unique_ptr<KeyboardLayoutProvider> provider, QObject* parent = nullptr);
  void start();
  [[nodiscard]] bool available() const { return !layoutCode().isEmpty(); }
  [[nodiscard]] QString layoutCode() const { return provider_ ? provider_->layoutCode() : QString{}; }
  [[nodiscard]] QString layoutName() const { return provider_ ? provider_->layoutName() : QString{}; }
 Q_SIGNALS:
  void layoutCodeChanged();
  void layoutNameChanged();

 private:
  std::unique_ptr<KeyboardLayoutProvider> provider_;
};
