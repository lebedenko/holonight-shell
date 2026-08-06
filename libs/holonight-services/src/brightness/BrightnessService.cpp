#include "BrightnessService.h"

#include "BrightnessBackend.h"
#include "NullBrightnessBackend.h"
#include "SysfsBackend.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcBrightnessService, "holonight.brightness")

namespace {
std::unique_ptr<BrightnessBackend> makeBrightnessBackend(QObject* parent) {
  auto candidate = std::make_unique<SysfsBackend>(parent);
  if (candidate->maxBrightness() > 0) {
    return candidate;
  }
  return std::make_unique<NullBrightnessBackend>(parent);
}
}  // namespace

BrightnessService::BrightnessService(QObject* parent) : QObject(parent), backend_(makeBrightnessBackend(this)) {
  init();
}

BrightnessService::BrightnessService(std::unique_ptr<BrightnessBackend> backend, QObject* parent)
    : QObject(parent), backend_(std::move(backend)) {
  init();
}

BrightnessService::~BrightnessService() = default;

void BrightnessService::init() {
  max_brightness_ = backend_->maxBrightness();
  brightness_percent_ = computePercent(backend_->currentBrightness());
  connect(backend_.get(), &BrightnessBackend::brightnessChanged, this, &BrightnessService::onBrightnessChanged);
  qCInfo(lcBrightnessService) << "BrightnessService ready; hasBacklight:" << hasBacklight()
                              << "initial:" << brightness_percent_ << "%";
}

int BrightnessService::computePercent(int raw) const {
  if (max_brightness_ == 0) {
    return 0;
  }
  const int percent = static_cast<int>(std::round(static_cast<double>(raw) * 100.0 / max_brightness_));
  return std::clamp(percent, 0, 100);
}

void BrightnessService::setBrightnessPercent(int percent) {
  if (max_brightness_ == 0) {
    return;
  }
  const int clamped_percent = std::clamp(percent, 0, 100);
  const int raw = static_cast<int>(std::round(static_cast<double>(clamped_percent) * max_brightness_ / 100.0));
  backend_->setBrightness(raw);
}

void BrightnessService::onBrightnessChanged(int new_raw) {
  const int new_pct = computePercent(new_raw);
  if (new_pct == brightness_percent_) {
    return;
  }
  brightness_percent_ = new_pct;
  emit brightnessPercentChanged(brightness_percent_);
}
