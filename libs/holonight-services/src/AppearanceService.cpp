#include "AppearanceService.h"

#include <QCoreApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAppearance, "holonight.appearance")

AppearanceService::AppearanceService(QObject* parent) : QObject(parent) {
  debug_overlays_ = QCoreApplication::arguments().contains(QStringLiteral("--debug-overlays"));
  applyAppearance(reader_.appearance());
  connect(&reader_, &Holonight::AppearanceReader::appearanceChanged, this,
          [this]() { applyAppearance(reader_.appearance()); });
}

void AppearanceService::applyAppearance(const Holonight::ResolvedAppearance& appearance) {
  if (ui_font_ != appearance.ui_font) {
    ui_font_ = appearance.ui_font;
    emit uiFontChanged();
  }
  if (fixed_font_ != appearance.monospace_font) {
    fixed_font_ = appearance.monospace_font;
    emit fixedFontChanged();
  }
  if (clock_font_ != appearance.display_font) {
    clock_font_ = appearance.display_font;
    emit clockFontChanged();
  }
  if (title_font_ != appearance.title_font) {
    title_font_ = appearance.title_font;
    emit titleFontChanged();
  }
  if (ui_font_size_ != appearance.ui_font_size) {
    ui_font_size_ = appearance.ui_font_size;
    emit uiFontSizeChanged();
  }
  if (fixed_font_size_ != appearance.monospace_font_size) {
    fixed_font_size_ = appearance.monospace_font_size;
    emit fixedFontSizeChanged();
  }
  if (clock_font_size_ != appearance.display_font_size) {
    clock_font_size_ = appearance.display_font_size;
    emit clockFontSizeChanged();
  }
  if (title_font_size_ != appearance.title_font_size) {
    title_font_size_ = appearance.title_font_size;
    emit titleFontSizeChanged();
  }
  qCInfo(lcAppearance) << "Appearance applied: uiFont=" << ui_font_ << "clockFontSize=" << clock_font_size_;
}
