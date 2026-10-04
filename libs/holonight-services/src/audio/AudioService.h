#pragma once

#include <QQmlEngine>

#include <AudioController.h>

using HoloNight::System::AudioDevice;
using HoloNight::System::AudioDeviceModel;
using HoloNight::System::AudioDeviceType;
using HoloNight::System::AudioHealthState;
using HoloNight::System::AudioStream;
using HoloNight::System::AudioStreamModel;
using HoloNight::System::AudioStreamType;

class AudioService final : public HoloNight::System::AudioController {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit AudioService(QObject* parent = nullptr) : AudioController(parent) {}
  explicit AudioService(SkipInitTag tag, QObject* parent = nullptr) : AudioController(tag, parent) {}
};

// Anonymous model metadata lets tools resolve inherited provider properties.
struct AudioDeviceModelForeign {
  Q_GADGET
  QML_FOREIGN(HoloNight::System::AudioDeviceModel)
  QML_ANONYMOUS
};
struct AudioStreamModelForeign {
  Q_GADGET
  QML_FOREIGN(HoloNight::System::AudioStreamModel)
  QML_ANONYMOUS
};
