#include "AppearanceService.h"

#include <QCoreApplication>
#include <QLoggingCategory>

#include <functional>

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

template <typename Value, typename Signal>
void emitIfChanged(bool notify, const Value& previous, const Value& current, AppearanceService* service,
                   Signal signal) {
  if (notify && previous != current) {
    std::invoke(signal, service);
  }
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

  emitIfChanged(notify, previous.scheme, appearance_.scheme, this, &AppearanceService::schemeChanged);
  emitIfChanged(notify, previous.accent, appearance_.accent, this, &AppearanceService::accentChanged);
  emitIfChanged(notify, previous.color_mode, appearance_.color_mode, this, &AppearanceService::colorModeChanged);
  emitIfChanged(notify, previous.ui_font, appearance_.ui_font, this, &AppearanceService::uiFontChanged);
  emitIfChanged(notify, previous.monospace_font, appearance_.monospace_font, this,
                &AppearanceService::monospaceFontChanged);
  emitIfChanged(notify, previous.title_font, appearance_.title_font, this, &AppearanceService::titleFontChanged);
  emitIfChanged(notify, previous.display_font, appearance_.display_font, this, &AppearanceService::displayFontChanged);
  emitIfChanged(notify, previous.ui_font_size, appearance_.ui_font_size, this, &AppearanceService::uiFontSizeChanged);
  emitIfChanged(notify, previous.monospace_font_size, appearance_.monospace_font_size, this,
                &AppearanceService::monospaceFontSizeChanged);
  emitIfChanged(notify, previous.title_font_size, appearance_.title_font_size, this,
                &AppearanceService::titleFontSizeChanged);
  emitIfChanged(notify, previous.display_font_size, appearance_.display_font_size, this,
                &AppearanceService::displayFontSizeChanged);
  emitIfChanged(notify, previous.icon_theme, appearance_.icon_theme, this, &AppearanceService::iconThemeChanged);
  emitIfChanged(notify, previous.fallback_icon_theme, appearance_.fallback_icon_theme, this,
                &AppearanceService::fallbackIconThemeChanged);
  emitIfChanged(notify, previous.cursor_theme, appearance_.cursor_theme, this, &AppearanceService::cursorThemeChanged);
  emitIfChanged(notify, previous.layout_scale, appearance_.layout_scale, this, &AppearanceService::layoutScaleChanged);
  emitIfChanged(notify, previous.shape_style, appearance_.shape_style, this, &AppearanceService::shapeStyleChanged);
  emitIfChanged(notify, previous.shape_scale, appearance_.shape_scale, this, &AppearanceService::shapeScaleChanged);
  emitIfChanged(notify, previous.base_radius, appearance_.base_radius, this, &AppearanceService::baseRadiusChanged);
  emitIfChanged(notify, previous.base_chamfer, appearance_.base_chamfer, this, &AppearanceService::baseChamferChanged);

  initialized_ = true;
  if (revision_ != revision) {
    revision_ = revision;
    emit revisionChanged();
  }
  qCInfo(lcAppearance) << "Appearance applied: scheme=" << appearance_.scheme << "revision=" << revision_;
}
