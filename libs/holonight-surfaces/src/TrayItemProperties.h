#pragma once

#include "TrayItem.h"

#include <QVariantMap>

[[nodiscard]] TrayItem mergeTrayItemProperties(const TrayItem* existing, const QString& service, const QString& path,
                                               const QVariantMap& changed);
