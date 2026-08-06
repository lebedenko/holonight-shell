#pragma once

#include "OsdEvent.h"

#include <QObject>
#include <QString>

// REQ-F-002. The one interface OsdController knows about. Concrete services (AudioService,
// BrightnessService, KeyboardLayoutService) are reached only through subclasses of this, so
// OsdController.cpp references none of them and the controller stays unit-testable with fakes.
class OsdChannelSource : public QObject {
  Q_OBJECT

 public:
  explicit OsdChannelSource(QObject* parent = nullptr) : QObject(parent) {}
  ~OsdChannelSource() override = default;

  OsdChannelSource(const OsdChannelSource&) = delete;
  OsdChannelSource& operator=(const OsdChannelSource&) = delete;
  OsdChannelSource(OsdChannelSource&&) = delete;
  OsdChannelSource& operator=(OsdChannelSource&&) = delete;

  // Stable identifier: "audio-volume" | "screen-brightness" | "keyboard-layout".
  [[nodiscard]] virtual QString channel() const = 0;

  // Synchronous snapshot, read once by OsdController immediately after it connects
  // availableChanged(). Sources whose availability is backed by a CONSTANT property with no
  // notify signal (BrightnessService::hasBacklight) have no way to report it otherwise: an
  // emission from their own constructor would predate the controller's connect() and be lost.
  [[nodiscard]] virtual bool isAvailable() const = 0;

 Q_SIGNALS:
  void eventObserved(const OsdEvent& event);
  void availableChanged(bool available);
};
