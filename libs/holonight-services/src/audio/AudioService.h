#pragma once

#include <QQmlEngine>

#include <AudioController.h>

using AudioDevice = HoloNight::System::AudioDevice;
using AudioDeviceModel = HoloNight::System::AudioDeviceModel;
using AudioDeviceType = HoloNight::System::AudioDeviceType;
using AudioHealthState = HoloNight::System::AudioHealthState;
using AudioStream = HoloNight::System::AudioStream;
using AudioStreamModel = HoloNight::System::AudioStreamModel;
using AudioStreamType = HoloNight::System::AudioStreamType;

class AudioService final : public HoloNight::System::AudioController {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit AudioService(QObject* parent = nullptr) : AudioController(parent) {}
  explicit AudioService(SkipInitTag tag, QObject* parent = nullptr) : AudioController(tag, parent) {}
};
