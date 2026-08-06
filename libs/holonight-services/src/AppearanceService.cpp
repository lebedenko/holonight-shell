#include "AppearanceService.h"

#include "ConfigService.h"

#include <QCoreApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAppearance, "holonight.appearance")

AppearanceService::AppearanceService(ConfigService* config, QObject* parent) : QObject(parent) {
  debug_overlays_ = QCoreApplication::arguments().contains(QStringLiteral("--debug-overlays"));
  applyAppearance(config != nullptr ? config->appearance() : AppearanceConfig{});
  if (config == nullptr) {
    return;
  }
  connect(config, &ConfigService::appearanceChanged, this, [this, config]() { applyAppearance(config->appearance()); });
}

void AppearanceService::applyAppearance(const AppearanceConfig& cfg) {
  if (ui_font_ != cfg.ui_font) {
    ui_font_ = cfg.ui_font;
    emit uiFontChanged();
  }
  if (fixed_font_ != cfg.fixed_font) {
    fixed_font_ = cfg.fixed_font;
    emit fixedFontChanged();
  }
  if (clock_font_ != cfg.clock_font) {
    clock_font_ = cfg.clock_font;
    emit clockFontChanged();
  }
  if (title_font_ != cfg.title_font) {
    title_font_ = cfg.title_font;
    emit titleFontChanged();
  }
  if (ui_font_size_ != cfg.ui_font_size) {
    ui_font_size_ = cfg.ui_font_size;
    emit uiFontSizeChanged();
  }
  if (fixed_font_size_ != cfg.fixed_font_size) {
    fixed_font_size_ = cfg.fixed_font_size;
    emit fixedFontSizeChanged();
  }
  if (clock_font_size_ != cfg.clock_font_size) {
    clock_font_size_ = cfg.clock_font_size;
    emit clockFontSizeChanged();
  }
  if (title_font_size_ != cfg.title_font_size) {
    title_font_size_ = cfg.title_font_size;
    emit titleFontSizeChanged();
  }
  qCInfo(lcAppearance) << "Appearance applied: uiFont=" << ui_font_ << "clockFontSize=" << clock_font_size_;
}
