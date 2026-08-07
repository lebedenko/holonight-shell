#include "AppearanceService.h"

#include <QCoreApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAppearance, "holonight.appearance")

namespace {

QString colorModeName(Holonight::ColorMode mode) {
  switch (mode) {
    case Holonight::ColorMode::Dark:
      return QStringLiteral("dark");
    case Holonight::ColorMode::Light:
      return QStringLiteral("light");
  }
  Q_UNREACHABLE_RETURN({});
}

QString shapeStyleName(Holonight::ResolvedShapeStyle style) {
  switch (style) {
    case Holonight::ResolvedShapeStyle::Inherit:
      return QStringLiteral("inherit");
    case Holonight::ResolvedShapeStyle::Hybrid:
      return QStringLiteral("hybrid");
    case Holonight::ResolvedShapeStyle::Rounded:
      return QStringLiteral("rounded");
    case Holonight::ResolvedShapeStyle::Chamfered:
      return QStringLiteral("chamfered");
  }
  Q_UNREACHABLE_RETURN({});
}

}  // namespace

AppearanceService::AppearanceService(QObject* parent) : QObject(parent) {
  debug_overlays_ = QCoreApplication::arguments().contains(QStringLiteral("--debug-overlays"));
  applyAppearance(reader_.appearance(), reader_.revision());
  connect(&reader_, &Holonight::AppearanceReader::appearanceChanged, this,
          [this]() { applyAppearance(reader_.appearance(), reader_.revision()); });
}

QString AppearanceService::colorMode() const { return colorModeName(appearance_.color_mode); }

QString AppearanceService::shapeStyle() const { return shapeStyleName(appearance_.shape_style); }

void AppearanceService::applyAppearance(const Holonight::ResolvedAppearance& appearance, int revision) {
  const auto previous = appearance_;
  const bool notify = initialized_;
  appearance_ = appearance;

  if (notify && previous.scheme != appearance_.scheme) emit schemeChanged();
  if (notify && previous.accent != appearance_.accent) emit accentChanged();
  if (notify && previous.color_mode != appearance_.color_mode) emit colorModeChanged();
  if (notify && previous.ui_font != appearance_.ui_font) emit uiFontChanged();
  if (notify && previous.monospace_font != appearance_.monospace_font) {
    emit monospaceFontChanged();
    emit fixedFontChanged();
  }
  if (notify && previous.title_font != appearance_.title_font) emit titleFontChanged();
  if (notify && previous.display_font != appearance_.display_font) {
    emit displayFontChanged();
    emit clockFontChanged();
  }
  if (notify && previous.ui_font_size != appearance_.ui_font_size) emit uiFontSizeChanged();
  if (notify && previous.monospace_font_size != appearance_.monospace_font_size) {
    emit monospaceFontSizeChanged();
    emit fixedFontSizeChanged();
  }
  if (notify && previous.title_font_size != appearance_.title_font_size) emit titleFontSizeChanged();
  if (notify && previous.display_font_size != appearance_.display_font_size) {
    emit displayFontSizeChanged();
    emit clockFontSizeChanged();
  }
  if (notify && previous.icon_theme != appearance_.icon_theme) emit iconThemeChanged();
  if (notify && previous.fallback_icon_theme != appearance_.fallback_icon_theme) emit fallbackIconThemeChanged();
  if (notify && previous.cursor_theme != appearance_.cursor_theme) emit cursorThemeChanged();
  if (notify && previous.layout_scale != appearance_.layout_scale) emit layoutScaleChanged();
  if (notify && previous.shape_style != appearance_.shape_style) emit shapeStyleChanged();
  if (notify && previous.shape_scale != appearance_.shape_scale) emit shapeScaleChanged();
  if (notify && previous.base_radius != appearance_.base_radius) emit baseRadiusChanged();
  if (notify && previous.base_chamfer != appearance_.base_chamfer) emit baseChamferChanged();

  initialized_ = true;
  if (revision_ != revision) {
    revision_ = revision;
    emit revisionChanged();
  }
  qCInfo(lcAppearance) << "Appearance applied: scheme=" << appearance_.scheme << "revision=" << revision_;
}
