#include "AuthenticationWindowGeometry.h"

#include <algorithm>

void AuthenticationWindowGeometry::setWindow(QQuickWindow* window) {
  if (window_ == window) {
    return;
  }
  if (window_) {
    disconnect(window_, nullptr, this, nullptr);
  }
  window_ = window;
  if (window_) {
    connect(window_, &QWindow::screenChanged, this, &AuthenticationWindowGeometry::trackScreen);
  }
  trackScreen();
  emit windowChanged();
}

void AuthenticationWindowGeometry::trackScreen() {
  if (screen_) {
    disconnect(screen_, nullptr, this, nullptr);
  }
  screen_ = window_ ? window_->screen() : nullptr;
  if (screen_) {
    connect(screen_, &QScreen::availableGeometryChanged, this, &AuthenticationWindowGeometry::reclamp);
    connect(screen_, &QScreen::geometryChanged, this, &AuthenticationWindowGeometry::reclamp);
  }
  reclamp();
}

void AuthenticationWindowGeometry::reclamp() {
  if (!window_ || !screen_) {
    return;
  }
  const QRect available = screen_->availableGeometry();
  const QRect bounds = available.isValid() ? available : screen_->geometry();
  const QSize maximum{std::max(1, bounds.width() - 48), std::max(1, bounds.height() - 48)};
  window_->setMaximumSize(maximum);
  // Screen changes may shrink the window; never undo a smaller compositor configure.
  window_->resize(window_->size().boundedTo(maximum));
}

void AuthenticationWindowGeometry::resetSize(int preferred_width, int preferred_height) {
  reclamp();
  if (window_) {
    window_->resize(QSize{preferred_width, preferred_height}.expandedTo(QSize{1, 1}).boundedTo(window_->maximumSize()));
  }
}
