#include "PulseAudioBackend.h"

#include "PulseAudioSystem.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QString>

#include <algorithm>
#include <pulse/pulseaudio.h>
#include <tuple>

Q_LOGGING_CATEGORY(lcAudioBackend, "holonight.audio.backend")

static constexpr uint32_t kVolumeNorm = PA_VOLUME_NORM;

static int clampPercent(int percent) { return std::clamp(percent, 0, 100); }

static uint32_t volumeFromPercent(int percent) {
  const int clamped = clampPercent(percent);
  return static_cast<uint32_t>(((static_cast<uint64_t>(clamped) * kVolumeNorm) + 50) / 100);
}

static uint8_t volumeToPercent(uint32_t raw) {
  const auto pct = static_cast<int>(((static_cast<uint64_t>(raw) * 100) + (kVolumeNorm / 2)) / kVolumeNorm);
  return static_cast<uint8_t>(clampPercent(pct));
}

static PulseAudioSystem* s_pa_system = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static RealPulseAudioSystem s_real_pa_system;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const std::vector<int> kDefaultReconnectBackoffMs = {1000, 2000, 4000, 8000, 16000, 30000};
static std::vector<int> s_reconnect_backoff_ms =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    kDefaultReconnectBackoffMs;

static PulseAudioSystem* pulseSystem() { return s_pa_system != nullptr ? s_pa_system : &s_real_pa_system; }

void PulseAudioBackend::setPulseAudioSystem(PulseAudioSystem* sys) { s_pa_system = sys; }

void PulseAudioBackend::resetPulseAudioSystem() { s_pa_system = nullptr; }

void PulseAudioBackend::setReconnectBackoffScheduleForTests(std::vector<int> delays_ms) {
  s_reconnect_backoff_ms = std::move(delays_ms);
}

void PulseAudioBackend::resetReconnectBackoffSchedule() { s_reconnect_backoff_ms = kDefaultReconnectBackoffMs; }

// ============================================================
struct PulseAudioBackend::Impl {
  struct VolumeRequest {
    int percent;
  };

  PulseAudioBackend* backend{nullptr};
  bool started{false};
  bool mainloop_running{false};
  pa_threaded_mainloop* mainloop{nullptr};
  pa_mainloop_api* api{nullptr};
  pa_context* context{nullptr};
  QString default_sink_name;
  QString default_source_name;

  [[nodiscard]] AudioDevice sinkToDevice(const pa_sink_info* info) const;
  [[nodiscard]] AudioDevice sourceToDevice(const pa_source_info* info) const;
  [[nodiscard]] static AudioStream sinkInputToStream(const pa_sink_input_info* info);
  [[nodiscard]] static AudioStream sourceOutputToStream(const pa_source_output_info* info);

  void onContextReady();
  void teardownContext();
  [[nodiscard]] bool connectNewContext();
  void queryServerInfo();
  void querySinks();
  void querySources();
  void querySinkInputs();
  void querySourceOutputs();

  void handleSinkEvent(pa_context* ctx, int evt, uint32_t idx);
  void handleSourceEvent(pa_context* ctx, int evt, uint32_t idx);
  void handleSinkInputEvent(pa_context* ctx, int evt, uint32_t idx);
  void handleSourceOutputEvent(pa_context* ctx, int evt, uint32_t idx);

  static void contextStateCallback(pa_context* ctx, void* userdata);
  static void subscribeCallback(pa_context* ctx, pa_subscription_event_type_t type, uint32_t idx, void* userdata);
  static void serverInfoCallback(pa_context* ctx, const pa_server_info* info, void* userdata);
  static void sinkListCallback(pa_context* ctx, const pa_sink_info* info, int eol, void* userdata);
  static void sourceListCallback(pa_context* ctx, const pa_source_info* info, int eol, void* userdata);
  static void sinkInputListCallback(pa_context* ctx, const pa_sink_input_info* info, int eol, void* userdata);
  static void sourceOutputListCallback(pa_context* ctx, const pa_source_output_info* info, int eol, void* userdata);
  static void sinkChangedCallback(pa_context* ctx, const pa_sink_info* info, int eol, void* userdata);
  static void sourceChangedCallback(pa_context* ctx, const pa_source_info* info, int eol, void* userdata);
  static void sinkInputChangedCallback(pa_context* ctx, const pa_sink_input_info* info, int eol, void* userdata);
  static void sourceOutputChangedCallback(pa_context* ctx, const pa_source_output_info* info, int eol, void* userdata);
  static void setSinkVolumeCallback(pa_context* ctx, const pa_sink_info* info, int eol, void* userdata);
  static void setSourceVolumeCallback(pa_context* ctx, const pa_source_info* info, int eol, void* userdata);
  static void setSinkInputVolumeCallback(pa_context* ctx, const pa_sink_input_info* info, int eol, void* userdata);
};

// ============================================================
// Conversion helpers
// ============================================================

AudioDevice PulseAudioBackend::Impl::sinkToDevice(const pa_sink_info* info) const {
  AudioDevice dev;
  dev.id = info->index;
  dev.name = QString::fromUtf8(info->name != nullptr ? info->name : "");
  dev.description = QString::fromUtf8(info->description != nullptr ? info->description : "");
  dev.volume = volumeToPercent(pa_cvolume_avg(&info->volume));
  dev.muted = info->mute != 0;
  dev.is_default = (dev.name == default_sink_name);
  dev.type = AudioDeviceType::Sink;
  return dev;
}

AudioDevice PulseAudioBackend::Impl::sourceToDevice(const pa_source_info* info) const {
  AudioDevice dev;
  dev.id = info->index;
  dev.name = QString::fromUtf8(info->name != nullptr ? info->name : "");
  dev.description = QString::fromUtf8(info->description != nullptr ? info->description : "");
  dev.volume = volumeToPercent(pa_cvolume_avg(&info->volume));
  dev.muted = info->mute != 0;
  dev.is_default = (dev.name == default_source_name);
  dev.type = AudioDeviceType::Source;
  return dev;
}

AudioStream PulseAudioBackend::Impl::sinkInputToStream(const pa_sink_input_info* info) {
  AudioStream stream;
  stream.id = info->index;
  stream.name = QString::fromUtf8(info->name != nullptr ? info->name : "");
  const char* app_name = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_NAME);
  stream.application = QString::fromUtf8(app_name != nullptr ? app_name : "");
  const char* icon = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_ICON_NAME);
  stream.icon_name = QString::fromUtf8(icon != nullptr ? icon : "");
  stream.device = info->sink;
  stream.volume = volumeToPercent(pa_cvolume_avg(&info->volume));
  stream.muted = info->mute != 0;
  stream.type = AudioStreamType::SinkInput;
  return stream;
}

AudioStream PulseAudioBackend::Impl::sourceOutputToStream(const pa_source_output_info* info) {
  AudioStream stream;
  stream.id = info->index;
  stream.name = QString::fromUtf8(info->name != nullptr ? info->name : "");
  const char* app_name = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_NAME);
  stream.application = QString::fromUtf8(app_name != nullptr ? app_name : "");
  const char* icon = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_ICON_NAME);
  stream.icon_name = QString::fromUtf8(icon != nullptr ? icon : "");
  stream.device = info->source;
  stream.volume = volumeToPercent(pa_cvolume_avg(&info->volume));
  stream.muted = info->mute != 0;
  stream.type = AudioStreamType::SourceOutput;
  return stream;
}

// ============================================================
// Context lifecycle
// ============================================================

void PulseAudioBackend::Impl::onContextReady() {
  const int subscription_mask = PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE |
                                PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
                                PA_SUBSCRIPTION_MASK_SERVER;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) — OR-ing valid bitmask constants
  const auto subscription_flags = static_cast<pa_subscription_mask_t>(subscription_mask);
  pa_operation* subscribe_operation =
      pulseSystem()->pa_context_subscribe(context, subscription_flags, nullptr, nullptr);
  if (subscribe_operation != nullptr) {
    pulseSystem()->pa_operation_unref(subscribe_operation);
  }

  queryServerInfo();

  PulseAudioBackend* self = backend;
  QMetaObject::invokeMethod(
      self,
      [self] {
        emit self->availableChanged(true);
        self->onReconnectSucceeded();
      },
      Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::teardownContext() {
  if (context == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(mainloop);
  pulseSystem()->pa_context_set_state_callback(context, nullptr, nullptr);
  pulseSystem()->pa_context_set_subscribe_callback(context, nullptr, nullptr);
  pulseSystem()->pa_context_disconnect(context);
  pulseSystem()->pa_context_unref(context);
  context = nullptr;
  pulseSystem()->threaded_mainloop_unlock(mainloop);
}

bool PulseAudioBackend::Impl::connectNewContext() {
  context = pulseSystem()->pa_context_new(api, "holonight-shell");
  if (context == nullptr) {
    qCWarning(lcAudioBackend) << "PulseAudioBackend: failed to create context";
    return false;
  }

  pulseSystem()->pa_context_set_state_callback(context, contextStateCallback, this);
  pulseSystem()->pa_context_set_subscribe_callback(context, subscribeCallback, this);

  pulseSystem()->threaded_mainloop_lock(mainloop);
  const bool connected = pulseSystem()->pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) >= 0;
  if (!connected) {
    qCWarning(lcAudioBackend) << "PulseAudioBackend: pa_context_connect failed:"
                              << pa_strerror(pulseSystem()->pa_context_errno(context));
  }
  pulseSystem()->threaded_mainloop_unlock(mainloop);
  return connected;
}

void PulseAudioBackend::Impl::queryServerInfo() {
  pa_operation* operation = pulseSystem()->pa_context_get_server_info(context, serverInfoCallback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::querySinks() {
  pa_operation* operation = pulseSystem()->pa_context_get_sink_info_list(context, sinkListCallback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::querySources() {
  pa_operation* operation = pulseSystem()->pa_context_get_source_info_list(context, sourceListCallback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::querySinkInputs() {
  pa_operation* operation = pulseSystem()->pa_context_get_sink_input_info_list(context, sinkInputListCallback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::querySourceOutputs() {
  pa_operation* operation =
      pulseSystem()->pa_context_get_source_output_info_list(context, sourceOutputListCallback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

// ============================================================
// Subscribe event dispatch
// ============================================================

void PulseAudioBackend::Impl::handleSinkEvent(pa_context* ctx, int evt, uint32_t idx) {
  if (evt == PA_SUBSCRIPTION_EVENT_REMOVE) {
    PulseAudioBackend* self = backend;
    QMetaObject::invokeMethod(self, [self, idx] { emit self->sinkRemoved(idx); }, Qt::QueuedConnection);
    return;
  }
  using SinkInfoCb = pa_sink_info_cb_t;
  SinkInfoCb callback = (evt == PA_SUBSCRIPTION_EVENT_NEW) ? sinkListCallback : sinkChangedCallback;
  pa_operation* operation = pulseSystem()->pa_context_get_sink_info_by_index(ctx, idx, callback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::handleSourceEvent(pa_context* ctx, int evt, uint32_t idx) {
  if (evt == PA_SUBSCRIPTION_EVENT_REMOVE) {
    PulseAudioBackend* self = backend;
    QMetaObject::invokeMethod(self, [self, idx] { emit self->sourceRemoved(idx); }, Qt::QueuedConnection);
    return;
  }
  using SourceInfoCb = pa_source_info_cb_t;
  SourceInfoCb callback = (evt == PA_SUBSCRIPTION_EVENT_NEW) ? sourceListCallback : sourceChangedCallback;
  pa_operation* operation = pulseSystem()->pa_context_get_source_info_by_index(ctx, idx, callback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::handleSinkInputEvent(pa_context* ctx, int evt, uint32_t idx) {
  if (evt == PA_SUBSCRIPTION_EVENT_REMOVE) {
    PulseAudioBackend* self = backend;
    QMetaObject::invokeMethod(self, [self, idx] { emit self->streamRemoved(idx); }, Qt::QueuedConnection);
    return;
  }
  using SinkInputInfoCb = pa_sink_input_info_cb_t;
  SinkInputInfoCb callback = (evt == PA_SUBSCRIPTION_EVENT_NEW) ? sinkInputListCallback : sinkInputChangedCallback;
  pa_operation* operation = pulseSystem()->pa_context_get_sink_input_info(ctx, idx, callback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::handleSourceOutputEvent(pa_context* ctx, int evt, uint32_t idx) {
  if (evt == PA_SUBSCRIPTION_EVENT_REMOVE) {
    PulseAudioBackend* self = backend;
    QMetaObject::invokeMethod(self, [self, idx] { emit self->streamRemoved(idx); }, Qt::QueuedConnection);
    return;
  }
  using SourceOutputInfoCb = pa_source_output_info_cb_t;
  SourceOutputInfoCb callback =
      (evt == PA_SUBSCRIPTION_EVENT_NEW) ? sourceOutputListCallback : sourceOutputChangedCallback;
  pa_operation* operation = pulseSystem()->pa_context_get_source_output_info(ctx, idx, callback, this);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

// ============================================================
// PA static callbacks
// ============================================================

void PulseAudioBackend::Impl::contextStateCallback(pa_context* ctx, void* userdata) {
  auto* impl = static_cast<Impl*>(userdata);
  switch (pulseSystem()->pa_context_get_state(ctx)) {
    case PA_CONTEXT_READY:
      impl->onContextReady();
      break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED: {
      PulseAudioBackend* self = impl->backend;
      QMetaObject::invokeMethod(
          self,
          [self] {
            emit self->availableChanged(false);
            self->onContextLost();
          },
          Qt::QueuedConnection);
      break;
    }
    default:
      break;
  }
}

void PulseAudioBackend::Impl::subscribeCallback(pa_context* ctx, pa_subscription_event_type_t type, uint32_t idx,
                                                void* userdata) {
  auto* impl = static_cast<Impl*>(userdata);
  const int facility = static_cast<int>(type) & static_cast<int>(PA_SUBSCRIPTION_EVENT_FACILITY_MASK);
  const int evt = static_cast<int>(type) & static_cast<int>(PA_SUBSCRIPTION_EVENT_TYPE_MASK);

  if (facility == PA_SUBSCRIPTION_EVENT_SINK) {
    impl->handleSinkEvent(ctx, evt, idx);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SOURCE) {
    impl->handleSourceEvent(ctx, evt, idx);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
    impl->handleSinkInputEvent(ctx, evt, idx);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT) {
    impl->handleSourceOutputEvent(ctx, evt, idx);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SERVER) {
    impl->queryServerInfo();
  }
}

void PulseAudioBackend::Impl::serverInfoCallback(pa_context* /*ctx*/, const pa_server_info* info, void* userdata) {
  if (info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  impl->default_sink_name = QString::fromUtf8(info->default_sink_name != nullptr ? info->default_sink_name : "");
  impl->default_source_name = QString::fromUtf8(info->default_source_name != nullptr ? info->default_source_name : "");
  impl->querySinks();
  impl->querySources();
  impl->querySinkInputs();
  impl->querySourceOutputs();
}

void PulseAudioBackend::Impl::sinkListCallback(pa_context* /*ctx*/, const pa_sink_info* info, int eol, void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioDevice dev = impl->sinkToDevice(info);
  PulseAudioBackend* self = impl->backend;
  QMetaObject::invokeMethod(self, [self, dev] { emit self->deviceAdded(dev); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sourceListCallback(pa_context* /*ctx*/, const pa_source_info* info, int eol,
                                                 void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  if (info->monitor_of_sink != PA_INVALID_INDEX) {
    return;  // skip monitor sources (loopback of a sink) — not real input devices
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioDevice dev = impl->sourceToDevice(info);
  PulseAudioBackend* self = impl->backend;
  QMetaObject::invokeMethod(self, [self, dev] { emit self->deviceAdded(dev); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sinkInputListCallback(pa_context* /*ctx*/, const pa_sink_input_info* info, int eol,
                                                    void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioStream stream = sinkInputToStream(info);
  PulseAudioBackend* self = impl->backend;
  QMetaObject::invokeMethod(self, [self, stream] { emit self->streamAdded(stream); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sourceOutputListCallback(pa_context* /*ctx*/, const pa_source_output_info* info, int eol,
                                                       void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioStream stream = sourceOutputToStream(info);
  PulseAudioBackend* self = impl->backend;
  QMetaObject::invokeMethod(self, [self, stream] { emit self->streamAdded(stream); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sinkChangedCallback(pa_context* /*ctx*/, const pa_sink_info* info, int eol,
                                                  void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioDevice dev = impl->sinkToDevice(info);
  PulseAudioBackend* self = impl->backend;
  QMetaObject::invokeMethod(self, [self, dev] { emit self->deviceChanged(dev); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sourceChangedCallback(pa_context* /*ctx*/, const pa_source_info* info, int eol,
                                                    void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  if (info->monitor_of_sink != PA_INVALID_INDEX) {
    return;  // skip monitor sources (loopback of a sink) — not real input devices
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioDevice dev = impl->sourceToDevice(info);
  PulseAudioBackend* self = impl->backend;
  QMetaObject::invokeMethod(self, [self, dev] { emit self->deviceChanged(dev); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sinkInputChangedCallback(pa_context* /*ctx*/, const pa_sink_input_info* info, int eol,
                                                       void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioStream stream = sinkInputToStream(info);
  PulseAudioBackend* self = impl->backend;
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  QMetaObject::invokeMethod(self, [self, stream] { emit self->streamChanged(stream); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::sourceOutputChangedCallback(pa_context* /*ctx*/, const pa_source_output_info* info,
                                                          int eol, void* userdata) {
  if (eol != 0 || info == nullptr) {
    return;
  }
  auto* impl = static_cast<Impl*>(userdata);
  AudioStream stream = sourceOutputToStream(info);
  PulseAudioBackend* self = impl->backend;
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  QMetaObject::invokeMethod(self, [self, stream] { emit self->streamChanged(stream); }, Qt::QueuedConnection);
}

void PulseAudioBackend::Impl::setSinkVolumeCallback(pa_context* ctx, const pa_sink_info* info, int eol,
                                                    void* userdata) {
  auto* request = static_cast<VolumeRequest*>(userdata);
  if (eol != 0 || info == nullptr) {
    delete request;
    return;
  }
  pa_cvolume volume;
  pa_cvolume_set(&volume, std::max(1U, static_cast<unsigned>(info->volume.channels)),
                 volumeFromPercent(request->percent));
  pa_operation* operation =
      pulseSystem()->pa_context_set_sink_volume_by_index(ctx, info->index, &volume, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::setSourceVolumeCallback(pa_context* ctx, const pa_source_info* info, int eol,
                                                      void* userdata) {
  auto* request = static_cast<VolumeRequest*>(userdata);
  if (eol != 0 || info == nullptr) {
    delete request;
    return;
  }
  pa_cvolume volume;
  pa_cvolume_set(&volume, std::max(1U, static_cast<unsigned>(info->volume.channels)),
                 volumeFromPercent(request->percent));
  pa_operation* operation =
      pulseSystem()->pa_context_set_source_volume_by_index(ctx, info->index, &volume, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

void PulseAudioBackend::Impl::setSinkInputVolumeCallback(pa_context* ctx, const pa_sink_input_info* info, int eol,
                                                         void* userdata) {
  auto* request = static_cast<VolumeRequest*>(userdata);
  if (eol != 0 || info == nullptr) {
    delete request;
    return;
  }
  pa_cvolume volume;
  pa_cvolume_set(&volume, std::max(1U, static_cast<unsigned>(info->volume.channels)),
                 volumeFromPercent(request->percent));
  pa_operation* operation =
      pulseSystem()->pa_context_set_sink_input_volume(ctx, info->index, &volume, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
}

// ============================================================
// PulseAudioBackend public methods
// ============================================================

PulseAudioBackend::PulseAudioBackend(QObject* parent)
    : QObject(parent), reconnect_timer_(new QTimer(this)), impl_(std::make_unique<Impl>()) {
  impl_->backend = this;
  reconnect_timer_->setSingleShot(true);
  connect(reconnect_timer_, &QTimer::timeout, this, &PulseAudioBackend::attemptReconnect);
}

PulseAudioBackend::~PulseAudioBackend() { stop(); }

void PulseAudioBackend::start() {
  if (impl_->started) {
    return;
  }
  impl_->started = true;

  impl_->mainloop = pulseSystem()->threaded_mainloop_new();
  impl_->api = impl_->mainloop != nullptr ? pulseSystem()->threaded_mainloop_get_api(impl_->mainloop) : nullptr;

  if (impl_->mainloop == nullptr) {
    qCWarning(lcAudioBackend) << "PulseAudioBackend: failed to create mainloop";
    return;
  }
  if (impl_->api == nullptr) {
    qCWarning(lcAudioBackend) << "PulseAudioBackend: failed to get mainloop api";
    return;
  }

  impl_->mainloop_running = pulseSystem()->threaded_mainloop_start(impl_->mainloop) >= 0;
  if (!impl_->mainloop_running) {
    qCWarning(lcAudioBackend) << "PulseAudioBackend: failed to start mainloop";
    return;
  }

  if (!impl_->connectNewContext()) {
    onContextLost();
  }
}

void PulseAudioBackend::onContextLost() {
  if (reconnect_timer_->isActive()) {
    return;
  }
  if (health_state_ == AudioHealthState::Failed) {
    return;
  }
  setHealthState(AudioHealthState::Reconnecting);
  scheduleReconnect();
}

void PulseAudioBackend::onReconnectSucceeded() {
  reconnect_attempt_ = 0;
  reconnect_timer_->stop();
  setHealthState(AudioHealthState::Connected);
}

void PulseAudioBackend::attemptReconnect() {
  impl_->teardownContext();
  if (!impl_->connectNewContext()) {
    onContextLost();
  }
}

void PulseAudioBackend::scheduleReconnect() {
  if (reconnect_attempt_ >= kMaxReconnectAttempts) {
    setHealthState(AudioHealthState::Failed);
    return;
  }
  const auto schedule_index = std::min(static_cast<size_t>(reconnect_attempt_), s_reconnect_backoff_ms.size() - 1);
  const int delay_ms = s_reconnect_backoff_ms[schedule_index];
  ++reconnect_attempt_;
  reconnect_timer_->start(delay_ms);
}

void PulseAudioBackend::setHealthState(AudioHealthState state) {
  if (health_state_ == state) {
    return;
  }
  health_state_ = state;
  emit healthStateChanged(health_state_);
}

void PulseAudioBackend::stop() {
  reconnect_timer_->stop();
  if (impl_->mainloop == nullptr) {
    return;
  }
  if (impl_->context != nullptr) {
    pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
    pulseSystem()->pa_context_set_state_callback(impl_->context, nullptr, nullptr);
    pulseSystem()->pa_context_set_subscribe_callback(impl_->context, nullptr, nullptr);
    pulseSystem()->pa_context_disconnect(impl_->context);
    pulseSystem()->pa_context_unref(impl_->context);
    pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
  }
  if (impl_->mainloop_running) {
    pulseSystem()->threaded_mainloop_stop(impl_->mainloop);
  }
  pulseSystem()->threaded_mainloop_free(impl_->mainloop);
  impl_->mainloop = nullptr;
  impl_->api = nullptr;
  impl_->context = nullptr;
  impl_->mainloop_running = false;
  impl_->started = false;
}

void PulseAudioBackend::setDeviceVolume(uint32_t idx, int percent) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  auto request = std::make_unique<Impl::VolumeRequest>(Impl::VolumeRequest{percent});
  auto* request_data = request.release();
  pa_operation* operation =
      pulseSystem()->pa_context_get_sink_info_by_index(impl_->context, idx, Impl::setSinkVolumeCallback, request_data);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  } else {
    delete request_data;
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setDeviceMuted(uint32_t idx, bool muted) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_sink_mute_by_index(impl_->context, idx, muted ? 1 : 0, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setSourceVolume(uint32_t idx, int percent) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  auto request = std::make_unique<Impl::VolumeRequest>(Impl::VolumeRequest{percent});
  auto* request_data = request.release();
  pa_operation* operation = pulseSystem()->pa_context_get_source_info_by_index(
      impl_->context, idx, Impl::setSourceVolumeCallback, request_data);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  } else {
    delete request_data;
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setSourceMuted(uint32_t idx, bool muted) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_source_mute_by_index(impl_->context, idx, muted ? 1 : 0, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setDefaultOutput(uint32_t idx) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  const QString name = QString::number(idx);
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_default_sink(impl_->context, name.toUtf8().constData(), nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setDefaultInput(uint32_t idx) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  const QString name = QString::number(idx);
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_default_source(impl_->context, name.toUtf8().constData(), nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setDefaultOutputByName(const QString& name) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_default_sink(impl_->context, name.toUtf8().constData(), nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setDefaultInputByName(const QString& name) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_default_source(impl_->context, name.toUtf8().constData(), nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setStreamVolume(uint32_t idx, int percent) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  auto request = std::make_unique<Impl::VolumeRequest>(Impl::VolumeRequest{percent});
  auto* request_data = request.release();
  pa_operation* operation = pulseSystem()->pa_context_get_sink_input_info(
      impl_->context, idx, Impl::setSinkInputVolumeCallback, request_data);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  } else {
    delete request_data;
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::setStreamMuted(uint32_t idx, bool muted) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_set_sink_input_mute(impl_->context, idx, muted ? 1 : 0, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}

void PulseAudioBackend::moveStreamToDevice(uint32_t stream_idx, uint32_t device_idx) {
  if (impl_->context == nullptr || impl_->mainloop == nullptr) {
    return;
  }
  pulseSystem()->threaded_mainloop_lock(impl_->mainloop);
  pa_operation* operation =
      pulseSystem()->pa_context_move_sink_input_by_index(impl_->context, stream_idx, device_idx, nullptr, nullptr);
  if (operation != nullptr) {
    pulseSystem()->pa_operation_unref(operation);
  }
  pulseSystem()->threaded_mainloop_unlock(impl_->mainloop);
}
