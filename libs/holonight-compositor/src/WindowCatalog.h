#pragma once

#include "CompositorSnapshot.h"

#include <QHash>

// Application rules depend only on the integration contract.
class WindowCatalog {
 public:
  void replace(const QList<CompositorWindow>& windows) {
    for (const auto& window : windows) {
      if (window.activated && !active_.contains(window.id)) {
        history_[window.id] = ++sequence_;
      }
    }
    active_.clear();
    for (const auto& window : windows) {
      if (window.activated) {
        active_.insert(window.id, true);
      }
    }
    windows_ = windows;
    for (auto it = history_.begin(); it != history_.end();) {
      if (find(it.key()) == nullptr) {
        it = history_.erase(it);
      } else {
        ++it;
      }
    }
  }
  [[nodiscard]] const CompositorWindow* find(const QString& identifier) const {
    for (const auto& window : windows_) {
      if (window.id == identifier) {
        return &window;
      }
    }
    return nullptr;
  }
  [[nodiscard]] WindowCommandResult validate(const QString& identifier, WindowCommand command, bool connected) const {
    if (!connected) {
      return WindowCommandResult::Disconnected;
    }
    const auto* window = find(identifier);
    if (window == nullptr) {
      return WindowCommandResult::InvalidWindow;
    }
    return window->operations.contains(command) ? WindowCommandResult::Accepted : WindowCommandResult::Unsupported;
  }
  [[nodiscard]] QList<CompositorWindow> search(const QString& query) const {
    QList<CompositorWindow> result;
    for (const auto& window : windows_) {
      if (window.title.contains(query, Qt::CaseInsensitive) || window.app_id.contains(query, Qt::CaseInsensitive)) {
        result.append(window);
      }
    }
    return result;
  }
  [[nodiscard]] quint64 activationOrder(const QString& identifier) const { return history_.value(identifier); }
  static QString groupKey(const CompositorWindow& window, bool grouped) {
    return grouped && !window.app_id.isEmpty() ? QStringLiteral("app:") + window.app_id
                                               : QStringLiteral("window:") + window.id;
  }

 private:
  QList<CompositorWindow> windows_;
  QHash<QString, bool> active_;
  QHash<QString, quint64> history_;
  quint64 sequence_{0};
};
