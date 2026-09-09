#pragma once

class QObject;

namespace holonight {
// Call after QGuiApplication consumes Qt options, before constructing any QML engine.
void configureQuickControls();
void reportQuickControlsLoaded(QObject* root);
}  // namespace holonight
