#include "AudioService.h"

#include "PulseAudioBackend.h"

#include <QCoreApplication>

AudioService::AudioService(QObject* parent)
    : QObject(parent),
      outputs_(new AudioDeviceModel(this)),
      inputs_(new AudioDeviceModel(this)),
      playback_streams_(new AudioStreamModel(this)),
      recording_streams_(new AudioStreamModel(this)),
      backend_(new PulseAudioBackend(this)) {}

AudioService::AudioService([[maybe_unused]] SkipInitTag tag, QObject* parent)
    : QObject(parent),
      outputs_(new AudioDeviceModel(this)),
      inputs_(new AudioDeviceModel(this)),
      playback_streams_(new AudioStreamModel(this)),
      recording_streams_(new AudioStreamModel(this)) {}

AudioService::~AudioService() {
  if (backend_ != nullptr) {
    backend_->stop();
  }
  QCoreApplication::removePostedEvents(this);
}

void AudioService::start() {
  if (started_) {
    return;
  }
  started_ = true;

  if (backend_ == nullptr) {
    return;
  }

  connect(backend_, &PulseAudioBackend::availableChanged, this, &AudioService::setAvailable);
  connect(backend_, &PulseAudioBackend::healthStateChanged, this, &AudioService::setHealthState);
  connect(backend_, &PulseAudioBackend::deviceAdded, this, &AudioService::onDeviceAdded);
  connect(backend_, &PulseAudioBackend::deviceChanged, this, &AudioService::onDeviceChanged);
  connect(backend_, &PulseAudioBackend::sinkRemoved, this, &AudioService::onSinkRemoved);
  connect(backend_, &PulseAudioBackend::sourceRemoved, this, &AudioService::onSourceRemoved);
  connect(backend_, &PulseAudioBackend::streamAdded, this, &AudioService::onStreamAdded);
  connect(backend_, &PulseAudioBackend::streamChanged, this, &AudioService::onStreamChanged);
  connect(backend_, &PulseAudioBackend::streamRemoved, this, &AudioService::onStreamRemoved);

  backend_->start();
}

void AudioService::setVolume(int percent) {
  if (backend_ == nullptr || default_output_id_ == kInvalidId) {
    return;
  }
  backend_->setDeviceVolume(default_output_id_, percent);
}

void AudioService::setDefaultOutput(uint32_t idx) {
  if (backend_ != nullptr) {
    backend_->setDefaultOutput(idx);
  }
}

void AudioService::setDefaultInput(uint32_t idx) {
  if (backend_ != nullptr) {
    backend_->setDefaultInput(idx);
  }
}

void AudioService::setDefaultOutputByName(const QString& name) {
  if (backend_ != nullptr) {
    backend_->setDefaultOutputByName(name);
  }
}

void AudioService::setDefaultInputByName(const QString& name) {
  if (backend_ != nullptr) {
    backend_->setDefaultInputByName(name);
  }
}

void AudioService::setDeviceVolume(uint32_t idx, int percent) {
  if (backend_ != nullptr) {
    backend_->setDeviceVolume(idx, percent);
  }
}

void AudioService::setDeviceMuted(uint32_t idx, bool muted) {
  if (backend_ != nullptr) {
    backend_->setDeviceMuted(idx, muted);
  }
}

void AudioService::setInputDeviceVolume(uint32_t idx, int percent) {
  if (backend_ != nullptr) {
    backend_->setSourceVolume(idx, percent);
  }
}

void AudioService::setInputDeviceMuted(uint32_t idx, bool muted) {
  if (backend_ != nullptr) {
    backend_->setSourceMuted(idx, muted);
  }
}

void AudioService::setStreamVolume(uint32_t idx, int percent) {
  if (backend_ != nullptr) {
    backend_->setStreamVolume(idx, percent);
  }
}

void AudioService::setStreamMuted(uint32_t idx, bool muted) {
  if (backend_ != nullptr) {
    backend_->setStreamMuted(idx, muted);
  }
}

void AudioService::moveStreamToOutput(uint32_t stream_idx, uint32_t sink_idx) {
  if (backend_ != nullptr) {
    backend_->moveStreamToDevice(stream_idx, sink_idx);
  }
}

void AudioService::moveStreamToInput(uint32_t stream_idx, uint32_t source_idx) {
  if (backend_ != nullptr) {
    backend_->moveStreamToDevice(stream_idx, source_idx);
  }
}

void AudioService::applyVolume(int value) {
  if (volume_ == value) {
    return;
  }
  volume_ = value;
  emit volumeChanged();
}

void AudioService::applyMuted(bool value) {
  if (muted_ == value) {
    return;
  }
  muted_ = value;
  emit mutedChanged();
}

void AudioService::setAvailable(bool value) {
  if (available_ == value) {
    return;
  }
  available_ = value;
  emit availableChanged();
}

void AudioService::onDeviceAdded(const AudioDevice& device) {
  AudioDeviceModel* model = (device.type == AudioDeviceType::Sink) ? outputs_ : inputs_;
  model->applyAdd(device);
  if (device.is_default) {
    applyDefaultDeviceState(device);
  }
}

void AudioService::onDeviceChanged(const AudioDevice& device) {
  AudioDeviceModel* model = (device.type == AudioDeviceType::Sink) ? outputs_ : inputs_;
  model->applyChange(device);
  if (device.is_default) {
    applyDefaultDeviceState(device);
  }
}

void AudioService::onSinkRemoved(uint32_t idx) {
  outputs_->applyRemove(idx);
  if (idx == default_output_id_) {
    default_output_id_ = kInvalidId;
    emit defaultOutputIdChanged();
  }
}

void AudioService::onSourceRemoved(uint32_t idx) { inputs_->applyRemove(idx); }

void AudioService::onStreamAdded(const AudioStream& stream) {
  AudioStreamModel* model = (stream.type == AudioStreamType::SinkInput) ? playback_streams_ : recording_streams_;
  model->applyAdd(stream);
}

void AudioService::onStreamChanged(const AudioStream& stream) {
  AudioStreamModel* model = (stream.type == AudioStreamType::SinkInput) ? playback_streams_ : recording_streams_;
  model->applyChange(stream);
}

void AudioService::onStreamRemoved(uint32_t idx) {
  playback_streams_->applyRemove(idx);
  recording_streams_->applyRemove(idx);
}

void AudioService::setHealthState(AudioHealthState value) {
  if (health_state_ == value) {
    return;
  }
  health_state_ = value;
  emit healthStateChanged();
}

void AudioService::applyDefaultDeviceState(const AudioDevice& device) {
  if (device.type == AudioDeviceType::Sink) {
    if (default_output_id_ != device.id) {
      default_output_id_ = device.id;
      emit defaultOutputIdChanged();
    }
    applyVolume(device.volume);
    applyMuted(device.muted);
  }
}
