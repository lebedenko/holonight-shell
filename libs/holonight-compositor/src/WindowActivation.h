#pragma once

#include <QList>
#include <QString>
#include <QtTypes>

#include <optional>

struct WindowActivationRequest {
  static constexpr qsizetype kMaximumLineageEntries = 64;
  static constexpr qsizetype kMaximumTitleHintBytes = 512;

  QList<quint32> process_lineage;
  QString title_hint;

  bool operator==(const WindowActivationRequest&) const = default;
};

struct WindowActivationCandidate {
  quint32 pid{0};
  QString title;

  bool operator==(const WindowActivationCandidate&) const = default;
};

enum class WindowActivationResult {
  Accepted,
  InvalidRequest,
  Unsupported,
  Disconnected,
  Missing,
  Ambiguous,
  Busy,
  Failed,
};

struct WindowActivationResolution {
  WindowActivationResult result{WindowActivationResult::Missing};
  std::optional<qsizetype> candidate_index;

  bool operator==(const WindowActivationResolution&) const = default;
};

[[nodiscard]] bool isValidWindowActivationRequest(const WindowActivationRequest& request);
[[nodiscard]] WindowActivationResolution resolveWindowActivation(const WindowActivationRequest& request,
                                                                 const QList<WindowActivationCandidate>& candidates);
