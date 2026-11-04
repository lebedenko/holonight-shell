#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
class WeatherIconBridge : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QVariantList previewCatalogue READ previewCatalogue CONSTANT)
 public:
  explicit WeatherIconBridge(QObject* parent = nullptr);
  QVariantList previewCatalogue() const;
  Q_INVOKABLE static QString iconFor(const QString& provider, int code, bool is_day, bool daylight_available = true);
  Q_INVOKABLE static QString moonPhaseIcon(const QVariant& moon_phase);
  Q_INVOKABLE static QString moonPhaseDescription(const QVariant& moon_phase);
};
