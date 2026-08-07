#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <holonight/appearance_reader.h>

class AppearanceService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QString scheme READ scheme NOTIFY schemeChanged)
  Q_PROPERTY(QString accent READ accent NOTIFY accentChanged)
  Q_PROPERTY(QString colorMode READ colorMode NOTIFY colorModeChanged)
  Q_PROPERTY(QString uiFont READ uiFont NOTIFY uiFontChanged)
  Q_PROPERTY(QString monospaceFont READ monospaceFont NOTIFY monospaceFontChanged)
  Q_PROPERTY(QString titleFont READ titleFont NOTIFY titleFontChanged)
  Q_PROPERTY(QString displayFont READ displayFont NOTIFY displayFontChanged)
  Q_PROPERTY(int uiFontSize READ uiFontSize NOTIFY uiFontSizeChanged)
  Q_PROPERTY(int monospaceFontSize READ monospaceFontSize NOTIFY monospaceFontSizeChanged)
  Q_PROPERTY(int titleFontSize READ titleFontSize NOTIFY titleFontSizeChanged)
  Q_PROPERTY(int displayFontSize READ displayFontSize NOTIFY displayFontSizeChanged)
  Q_PROPERTY(QString iconTheme READ iconTheme NOTIFY iconThemeChanged)
  Q_PROPERTY(QString fallbackIconTheme READ fallbackIconTheme NOTIFY fallbackIconThemeChanged)
  Q_PROPERTY(QString cursorTheme READ cursorTheme NOTIFY cursorThemeChanged)
  Q_PROPERTY(qreal layoutScale READ layoutScale NOTIFY layoutScaleChanged)
  Q_PROPERTY(QString shapeStyle READ shapeStyle NOTIFY shapeStyleChanged)
  Q_PROPERTY(qreal shapeScale READ shapeScale NOTIFY shapeScaleChanged)
  Q_PROPERTY(bool hasBaseRadius READ hasBaseRadius NOTIFY baseRadiusChanged)
  Q_PROPERTY(qreal baseRadius READ baseRadius NOTIFY baseRadiusChanged)
  Q_PROPERTY(bool hasBaseChamfer READ hasBaseChamfer NOTIFY baseChamferChanged)
  Q_PROPERTY(qreal baseChamfer READ baseChamfer NOTIFY baseChamferChanged)
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

  Q_PROPERTY(bool debugOverlays READ debugOverlays CONSTANT)

 public:
  explicit AppearanceService(QObject* parent = nullptr);
  ~AppearanceService() override = default;

  AppearanceService(const AppearanceService&) = delete;
  AppearanceService& operator=(const AppearanceService&) = delete;
  AppearanceService(AppearanceService&&) = delete;
  AppearanceService& operator=(AppearanceService&&) = delete;

  [[nodiscard]] QString scheme() const { return appearance_.scheme; }
  [[nodiscard]] QString accent() const { return appearance_.accent; }
  [[nodiscard]] QString colorMode() const;
  [[nodiscard]] QString uiFont() const { return appearance_.ui_font; }
  [[nodiscard]] QString monospaceFont() const { return appearance_.monospace_font; }
  [[nodiscard]] QString titleFont() const { return appearance_.title_font; }
  [[nodiscard]] QString displayFont() const { return appearance_.display_font; }
  [[nodiscard]] int uiFontSize() const { return appearance_.ui_font_size; }
  [[nodiscard]] int monospaceFontSize() const { return appearance_.monospace_font_size; }
  [[nodiscard]] int titleFontSize() const { return appearance_.title_font_size; }
  [[nodiscard]] int displayFontSize() const { return appearance_.display_font_size; }
  [[nodiscard]] QString iconTheme() const { return appearance_.icon_theme; }
  [[nodiscard]] QString fallbackIconTheme() const { return appearance_.fallback_icon_theme; }
  [[nodiscard]] QString cursorTheme() const { return appearance_.cursor_theme; }
  [[nodiscard]] qreal layoutScale() const { return appearance_.layout_scale; }
  [[nodiscard]] QString shapeStyle() const;
  [[nodiscard]] qreal shapeScale() const { return appearance_.shape_scale; }
  [[nodiscard]] bool hasBaseRadius() const { return appearance_.base_radius.has_value(); }
  [[nodiscard]] qreal baseRadius() const { return appearance_.base_radius.value_or(0.0); }
  [[nodiscard]] bool hasBaseChamfer() const { return appearance_.base_chamfer.has_value(); }
  [[nodiscard]] qreal baseChamfer() const { return appearance_.base_chamfer.value_or(0.0); }
  [[nodiscard]] int revision() const { return revision_; }
  [[nodiscard]] bool debugOverlays() const { return debug_overlays_; }
  [[nodiscard]] const Holonight::ResolvedAppearance& resolvedAppearance() const noexcept { return appearance_; }

 Q_SIGNALS:
  void schemeChanged();
  void accentChanged();
  void colorModeChanged();
  void uiFontChanged();
  void monospaceFontChanged();
  void titleFontChanged();
  void displayFontChanged();
  void uiFontSizeChanged();
  void monospaceFontSizeChanged();
  void titleFontSizeChanged();
  void displayFontSizeChanged();
  void iconThemeChanged();
  void fallbackIconThemeChanged();
  void cursorThemeChanged();
  void layoutScaleChanged();
  void shapeStyleChanged();
  void shapeScaleChanged();
  void baseRadiusChanged();
  void baseChamferChanged();
  void revisionChanged();

 private:
  void applyAppearance(const Holonight::ResolvedAppearance& appearance, int revision);

  Holonight::ResolvedAppearance appearance_;
  int revision_{0};
  bool initialized_{false};
  bool debug_overlays_{false};
  Holonight::AppearanceReader reader_;
};
