#include "CompositorSelection.h"

#include <QList>
#include <QString>

CompositorKind selectCompositor(const CompositorEnvironment& environment) {
  bool declares_hyprland = false;
  bool declares_sway = false;
  for (const QByteArray& token : environment.current_desktop.split(':')) {
    const QString desktop = QString::fromUtf8(token).trimmed();
    declares_hyprland |= desktop.compare(QStringLiteral("Hyprland"), Qt::CaseInsensitive) == 0;
    declares_sway |= desktop.compare(QStringLiteral("Sway"), Qt::CaseInsensitive) == 0;
  }

  if (declares_hyprland || declares_sway) {
    if (declares_hyprland == declares_sway) {
      return CompositorKind::Generic;
    }
    return declares_hyprland ? CompositorKind::Hyprland : CompositorKind::Sway;
  }

  const bool marks_hyprland = !environment.hyprland_instance_signature.isEmpty();
  const bool marks_sway = !environment.sway_socket.isEmpty();
  if (marks_hyprland == marks_sway) {
    return CompositorKind::Generic;
  }
  return marks_hyprland ? CompositorKind::Hyprland : CompositorKind::Sway;
}

CompositorEnvironment systemCompositorEnvironment() {
  return {
      .current_desktop = qgetenv("XDG_CURRENT_DESKTOP"),
      .hyprland_instance_signature = qgetenv("HYPRLAND_INSTANCE_SIGNATURE"),
      .sway_socket = qgetenv("SWAYSOCK"),
  };
}

const char* compositorName(CompositorKind kind) {
  switch (kind) {
    case CompositorKind::Hyprland:
      return "Hyprland";
    case CompositorKind::Sway:
      return "Sway";
    case CompositorKind::Generic:
      return "Wayland";
  }
  return "Wayland";
}
