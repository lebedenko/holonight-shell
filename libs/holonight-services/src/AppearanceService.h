#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class ConfigService;
struct AppearanceConfig;

class AppearanceService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QString uiFont READ uiFont NOTIFY uiFontChanged)
  Q_PROPERTY(QString fixedFont READ fixedFont NOTIFY fixedFontChanged)
  Q_PROPERTY(QString clockFont READ clockFont NOTIFY clockFontChanged)
  Q_PROPERTY(QString titleFont READ titleFont NOTIFY titleFontChanged)
  Q_PROPERTY(int uiFontSize READ uiFontSize NOTIFY uiFontSizeChanged)
  Q_PROPERTY(int fixedFontSize READ fixedFontSize NOTIFY fixedFontSizeChanged)
  Q_PROPERTY(int clockFontSize READ clockFontSize NOTIFY clockFontSizeChanged)
  Q_PROPERTY(int titleFontSize READ titleFontSize NOTIFY titleFontSizeChanged)
  Q_PROPERTY(bool debugOverlays READ debugOverlays CONSTANT)

 public:
  explicit AppearanceService(ConfigService* config, QObject* parent = nullptr);
  ~AppearanceService() override = default;

  AppearanceService(const AppearanceService&) = delete;
  AppearanceService& operator=(const AppearanceService&) = delete;
  AppearanceService(AppearanceService&&) = delete;
  AppearanceService& operator=(AppearanceService&&) = delete;

  [[nodiscard]] QString uiFont() const { return ui_font_; }
  [[nodiscard]] QString fixedFont() const { return fixed_font_; }
  [[nodiscard]] QString clockFont() const { return clock_font_; }
  [[nodiscard]] QString titleFont() const { return title_font_; }
  [[nodiscard]] int uiFontSize() const { return ui_font_size_; }
  [[nodiscard]] int fixedFontSize() const { return fixed_font_size_; }
  [[nodiscard]] int clockFontSize() const { return clock_font_size_; }
  [[nodiscard]] int titleFontSize() const { return title_font_size_; }
  [[nodiscard]] bool debugOverlays() const { return debug_overlays_; }

 Q_SIGNALS:
  void uiFontChanged();
  void fixedFontChanged();
  void clockFontChanged();
  void titleFontChanged();
  void uiFontSizeChanged();
  void fixedFontSizeChanged();
  void clockFontSizeChanged();
  void titleFontSizeChanged();

 private:
  void applyAppearance(const AppearanceConfig& cfg);

  QString ui_font_;
  QString fixed_font_;
  QString clock_font_;
  QString title_font_;
  int ui_font_size_{0};
  int fixed_font_size_{0};
  int clock_font_size_{0};
  int title_font_size_{0};
  bool debug_overlays_{false};
};
