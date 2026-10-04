#include "KeyboardLayoutService.h"
KeyboardLayoutService::KeyboardLayoutService(QObject* parent) : KeyboardLayoutService({}, parent) {}
KeyboardLayoutService::KeyboardLayoutService(std::unique_ptr<KeyboardLayoutProvider> provider, QObject* parent)
    : QObject(parent), provider_(std::move(provider)) {
  if (provider_) {
    connect(provider_.get(), &KeyboardLayoutProvider::layoutCodeChanged, this,
            &KeyboardLayoutService::layoutCodeChanged);
    connect(provider_.get(), &KeyboardLayoutProvider::layoutNameChanged, this,
            &KeyboardLayoutService::layoutNameChanged);
  }
}
void KeyboardLayoutService::start() {
  if (provider_) {
    provider_->start();
  }
}
