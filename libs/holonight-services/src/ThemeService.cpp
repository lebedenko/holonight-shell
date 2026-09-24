#include "ThemeService.h"

#include "AppearanceService.h"
#include "SettingsPortalBackend.h"

namespace {

const Holonight::ResolvedAppearance& requireAppearance(AppearanceService* appearance) {
  if (appearance == nullptr) {
    qFatal("ThemeService requires AppearanceService");
  }
  return appearance->resolvedAppearance();
}

}  // namespace

ThemeService::ThemeService(AppearanceService* appearance, QObject* parent)
    : QObject(parent),
      appearance_(appearance),
      settings_portal_backend_(new SettingsPortalBackend(requireAppearance(appearance), this)) {
  connect(appearance_, &AppearanceService::schemeChanged, this, &ThemeService::syncPortalAppearance);
  connect(appearance_, &AppearanceService::accentChanged, this, &ThemeService::syncPortalAppearance);
  connect(appearance_, &AppearanceService::colorModeChanged, this, &ThemeService::syncPortalAppearance);
}

void ThemeService::syncPortalAppearance() {
  settings_portal_backend_->applyAppearance(appearance_->resolvedAppearance());
}
