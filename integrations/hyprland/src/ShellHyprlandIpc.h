#pragma once
#include <QByteArray>
#include <QString>

#include <optional>
struct ShellHyprlandKeyboardLayout {
  QString keyboard_name;
  QString layout_name;
};
std::optional<ShellHyprlandKeyboardLayout> parseShellHyprlandKeyboardLayoutEvent(const QByteArray& line);
std::optional<QString> parseShellHyprlandKeyboardLayoutDevicesJson(const QByteArray& response);
QString shellKeyboardLayoutCode(const QString& layout_name);
