#include "PulseAudioBackend.h"
#include "PulseAudioSystem.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>
#include <vector>

// NOLINTBEGIN(readability-named-parameter,readability-identifier-length,cppcoreguidelines-pro-type-reinterpret-cast)
class FakePulseAudioSystem : public PulseAudioSystem {
 public:
  pa_threaded_mainloop* mock_mainloop = reinterpret_cast<pa_threaded_mainloop*>(0x1111);
  pa_mainloop_api* mock_api = reinterpret_cast<pa_mainloop_api*>(0x2222);
  pa_context* mock_context = reinterpret_cast<pa_context*>(0x3333);
  pa_operation* mock_operation = reinterpret_cast<pa_operation*>(0x4444);

  pa_context_notify_cb_t state_cb = nullptr;
  void* state_userdata = nullptr;
  pa_context_subscribe_cb_t subscribe_cb = nullptr;
  void* subscribe_userdata = nullptr;

  pa_context_state_t context_state = PA_CONTEXT_UNCONNECTED;
  pa_context* context_to_return = mock_context;
  int connect_result = 0;

  // Track function calls
  int start_calls = 0;
  int stop_calls = 0;
  int lock_calls = 0;
  int unlock_calls = 0;
  int connect_calls = 0;
  int disconnect_calls = 0;
  int subscribe_calls = 0;
  int get_server_info_calls = 0;
  int get_sink_list_calls = 0;
  int get_source_list_calls = 0;
  int get_sink_input_list_calls = 0;
  int get_source_output_list_calls = 0;

  // Control action variables
  uint32_t last_sink_idx = 0;
  int last_sink_volume = 0;
  unsigned last_sink_channels = 0;
  bool last_sink_muted = false;
  uint32_t last_source_idx = 0;
  int last_source_volume = 0;
  unsigned last_source_channels = 0;
  bool last_source_muted = false;
  QString last_default_sink;
  QString last_default_source;
  uint32_t last_stream_idx = 0;
  int last_stream_volume = 0;
  unsigned last_stream_channels = 0;
  bool last_stream_muted = false;
  uint32_t last_move_stream_idx = 0;
  uint32_t last_move_device_idx = 0;

  pa_threaded_mainloop* threaded_mainloop_new() override { return mock_mainloop; }
  pa_mainloop_api* threaded_mainloop_get_api(pa_threaded_mainloop*) override { return mock_api; }
  int threaded_mainloop_start(pa_threaded_mainloop*) override {
    start_calls++;
    return 0;
  }
  void threaded_mainloop_stop(pa_threaded_mainloop*) override { stop_calls++; }
  void threaded_mainloop_free(pa_threaded_mainloop*) override {}
  void threaded_mainloop_lock(pa_threaded_mainloop*) override { lock_calls++; }
  void threaded_mainloop_unlock(pa_threaded_mainloop*) override { unlock_calls++; }

  pa_context* pa_context_new(pa_mainloop_api*, const char*) override { return context_to_return; }
  void pa_context_set_state_callback(pa_context*, pa_context_notify_cb_t cb, void* userdata) override {
    state_cb = cb;
    state_userdata = userdata;
  }
  void pa_context_set_subscribe_callback(pa_context*, pa_context_subscribe_cb_t cb, void* userdata) override {
    subscribe_cb = cb;
    subscribe_userdata = userdata;
  }
  int pa_context_connect(pa_context*, const char*, pa_context_flags_t, const pa_spawn_api*) override {
    connect_calls++;
    return connect_result;
  }
  void pa_context_disconnect(pa_context*) override { disconnect_calls++; }
  void pa_context_unref(pa_context*) override {}

  pa_operation* pa_context_subscribe(pa_context*, pa_subscription_mask_t, pa_context_success_cb_t, void*) override {
    subscribe_calls++;
    return mock_operation;
  }
  pa_operation* pa_context_get_server_info(pa_context*, pa_server_info_cb_t cb, void* userdata) override {
    get_server_info_calls++;
    server_info_cb = cb;
    server_info_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_sink_info_list(pa_context*, pa_sink_info_cb_t cb, void* userdata) override {
    get_sink_list_calls++;
    sink_cb = cb;
    sink_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_source_info_list(pa_context*, pa_source_info_cb_t cb, void* userdata) override {
    get_source_list_calls++;
    source_cb = cb;
    source_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_sink_input_info_list(pa_context*, pa_sink_input_info_cb_t cb, void* userdata) override {
    get_sink_input_list_calls++;
    sink_input_cb = cb;
    sink_input_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_source_output_info_list(pa_context*, pa_source_output_info_cb_t cb,
                                                       void* userdata) override {
    get_source_output_list_calls++;
    source_output_cb = cb;
    source_output_userdata = userdata;
    return mock_operation;
  }

  // Callbacks capture
  pa_server_info_cb_t server_info_cb = nullptr;
  void* server_info_userdata = nullptr;
  pa_sink_info_cb_t sink_cb = nullptr;
  void* sink_userdata = nullptr;
  pa_source_info_cb_t source_cb = nullptr;
  void* source_userdata = nullptr;
  pa_sink_input_info_cb_t sink_input_cb = nullptr;
  void* sink_input_userdata = nullptr;
  pa_source_output_info_cb_t source_output_cb = nullptr;
  void* source_output_userdata = nullptr;

  pa_sink_info_cb_t sink_by_idx_cb = nullptr;
  void* sink_by_idx_userdata = nullptr;
  pa_source_info_cb_t source_by_idx_cb = nullptr;
  void* source_by_idx_userdata = nullptr;
  pa_sink_input_info_cb_t sink_input_by_idx_cb = nullptr;
  void* sink_input_by_idx_userdata = nullptr;
  pa_source_output_info_cb_t source_output_by_idx_cb = nullptr;
  void* source_output_by_idx_userdata = nullptr;

  pa_operation* pa_context_get_sink_info_by_index(pa_context*, uint32_t idx, pa_sink_info_cb_t cb,
                                                  void* userdata) override {
    sink_by_idx_cb = cb;
    sink_by_idx_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_source_info_by_index(pa_context*, uint32_t idx, pa_source_info_cb_t cb,
                                                    void* userdata) override {
    source_by_idx_cb = cb;
    source_by_idx_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_sink_input_info(pa_context*, uint32_t idx, pa_sink_input_info_cb_t cb,
                                               void* userdata) override {
    sink_input_by_idx_cb = cb;
    sink_input_by_idx_userdata = userdata;
    return mock_operation;
  }
  pa_operation* pa_context_get_source_output_info(pa_context*, uint32_t idx, pa_source_output_info_cb_t cb,
                                                  void* userdata) override {
    source_output_by_idx_cb = cb;
    source_output_by_idx_userdata = userdata;
    return mock_operation;
  }

  pa_operation* pa_context_set_sink_volume_by_index(pa_context*, uint32_t idx, const pa_cvolume* vol,
                                                    pa_context_success_cb_t, void*) override {
    last_sink_idx = idx;
    last_sink_channels = vol->channels;
    last_sink_volume =
        static_cast<int>(((static_cast<uint64_t>(vol->values[0]) * 100) + (PA_VOLUME_NORM / 2)) / PA_VOLUME_NORM);
    return mock_operation;
  }
  pa_operation* pa_context_set_sink_mute_by_index(pa_context*, uint32_t idx, int mute, pa_context_success_cb_t,
                                                  void*) override {
    last_sink_idx = idx;
    last_sink_muted = (mute != 0);
    return mock_operation;
  }
  pa_operation* pa_context_set_source_volume_by_index(pa_context*, uint32_t idx, const pa_cvolume* vol,
                                                      pa_context_success_cb_t, void*) override {
    last_source_idx = idx;
    last_source_channels = vol->channels;
    last_source_volume =
        static_cast<int>(((static_cast<uint64_t>(vol->values[0]) * 100) + (PA_VOLUME_NORM / 2)) / PA_VOLUME_NORM);
    return mock_operation;
  }
  pa_operation* pa_context_set_source_mute_by_index(pa_context*, uint32_t idx, int mute, pa_context_success_cb_t,
                                                    void*) override {
    last_source_idx = idx;
    last_source_muted = (mute != 0);
    return mock_operation;
  }

  pa_operation* pa_context_set_default_sink(pa_context*, const char* name, pa_context_success_cb_t, void*) override {
    last_default_sink = QString::fromUtf8(name);
    return mock_operation;
  }
  pa_operation* pa_context_set_default_source(pa_context*, const char* name, pa_context_success_cb_t, void*) override {
    last_default_source = QString::fromUtf8(name);
    return mock_operation;
  }

  pa_operation* pa_context_set_sink_input_volume(pa_context*, uint32_t idx, const pa_cvolume* vol,
                                                 pa_context_success_cb_t, void*) override {
    last_stream_idx = idx;
    last_stream_channels = vol->channels;
    last_stream_volume =
        static_cast<int>(((static_cast<uint64_t>(vol->values[0]) * 100) + (PA_VOLUME_NORM / 2)) / PA_VOLUME_NORM);
    return mock_operation;
  }
  pa_operation* pa_context_set_sink_input_mute(pa_context*, uint32_t idx, int mute, pa_context_success_cb_t,
                                               void*) override {
    last_stream_idx = idx;
    last_stream_muted = (mute != 0);
    return mock_operation;
  }
  pa_operation* pa_context_move_sink_input_by_index(pa_context*, uint32_t idx, uint32_t device_idx,
                                                    pa_context_success_cb_t, void*) override {
    last_move_stream_idx = idx;
    last_move_device_idx = device_idx;
    return mock_operation;
  }

  void pa_operation_unref(pa_operation*) override {}
  pa_context_state_t pa_context_get_state(const pa_context*) override { return context_state; }
  int pa_context_errno(const pa_context*) override { return 0; }
};
// NOLINTEND(readability-named-parameter,readability-identifier-length,cppcoreguidelines-pro-type-reinterpret-cast)

TEST(PulseAudioBackend, ConstructAndDestroyIsClean) { PulseAudioBackend backend; }

TEST(PulseAudioBackend, StopWithoutStartIsNoop) {
  PulseAudioBackend backend;
  backend.stop();
  backend.stop();
}

TEST(PulseAudioBackend, ControlMethodsAreNopWithoutStart) {
  PulseAudioBackend backend;
  backend.setDeviceVolume(0, 50);
  backend.setDeviceMuted(0, true);
  backend.setSourceVolume(0, 50);
  backend.setSourceMuted(0, true);
  backend.setDefaultOutput(0);
  backend.setDefaultInput(0);
  backend.setStreamVolume(0, 50);
  backend.setStreamMuted(0, false);
  backend.moveStreamToDevice(0, 1);
}

TEST(PulseAudioBackend, StartTransitionToReadyAndQueriesInfo) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    QSignalSpy available_spy(&backend, &PulseAudioBackend::availableChanged);

    backend.start();
    EXPECT_EQ(mock_sys.start_calls, 1);
    EXPECT_EQ(mock_sys.connect_calls, 1);
    ASSERT_NE(mock_sys.state_cb, nullptr);

    // Transition to ready
    mock_sys.context_state = PA_CONTEXT_READY;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);

    QCoreApplication::processEvents();

    // Verify it queried server info and emitted availableChanged
    EXPECT_EQ(mock_sys.subscribe_calls, 1);
    EXPECT_EQ(mock_sys.get_server_info_calls, 1);
    EXPECT_EQ(available_spy.count(), 1);
    EXPECT_TRUE(available_spy.first().at(0).toBool());

    backend.stop();
    EXPECT_EQ(mock_sys.stop_calls, 1);
    EXPECT_EQ(mock_sys.disconnect_calls, 1);
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, StartIsIdempotent) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    backend.start();
    backend.start();

    EXPECT_EQ(mock_sys.start_calls, 1);
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, StartAndStopCanBeCalledRepeatedly) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    backend.start();
    EXPECT_EQ(mock_sys.start_calls, 1);

    backend.stop();
    EXPECT_EQ(mock_sys.stop_calls, 1);

    backend.start();
    EXPECT_EQ(mock_sys.start_calls, 2);
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, HandlesStateFailures) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    QSignalSpy available_spy(&backend, &PulseAudioBackend::availableChanged);

    backend.start();
    ASSERT_NE(mock_sys.state_cb, nullptr);

    // Transition to failed
    mock_sys.context_state = PA_CONTEXT_FAILED;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);

    QCoreApplication::processEvents();

    EXPECT_EQ(available_spy.count(), 1);
    EXPECT_FALSE(available_spy.first().at(0).toBool());
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, ParsesAndEmitsDevicesAndStreams) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    QSignalSpy device_added_spy(&backend, &PulseAudioBackend::deviceAdded);
    QSignalSpy stream_added_spy(&backend, &PulseAudioBackend::streamAdded);

    backend.start();
    mock_sys.context_state = PA_CONTEXT_READY;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);

    pa_server_info info{};
    info.default_sink_name = "test-sink";
    info.default_source_name = "test-source";
    ASSERT_NE(mock_sys.server_info_cb, nullptr);
    mock_sys.server_info_cb(mock_sys.mock_context, &info, mock_sys.server_info_userdata);

    EXPECT_EQ(mock_sys.get_sink_list_calls, 1);
    EXPECT_EQ(mock_sys.get_source_list_calls, 1);
    EXPECT_EQ(mock_sys.get_sink_input_list_calls, 1);
    EXPECT_EQ(mock_sys.get_source_output_list_calls, 1);

    // Invoke sink list callback with a test sink
    pa_sink_info sink{};
    sink.index = 5;
    sink.name = "test-sink";
    sink.description = "Test Sink Device";
    sink.volume.channels = 2;
    sink.volume.values[0] = PA_VOLUME_NORM / 2;  // 50%
    sink.volume.values[1] = PA_VOLUME_NORM / 2;
    sink.mute = 0;

    ASSERT_NE(mock_sys.sink_cb, nullptr);
    mock_sys.sink_cb(mock_sys.mock_context, &sink, 0, mock_sys.sink_userdata);

    QCoreApplication::processEvents();

    ASSERT_EQ(device_added_spy.count(), 1);
    auto dev = device_added_spy.first().at(0).value<AudioDevice>();
    EXPECT_EQ(dev.id, 5U);
    EXPECT_EQ(dev.name, QStringLiteral("test-sink"));
    EXPECT_EQ(dev.description, QStringLiteral("Test Sink Device"));
    EXPECT_EQ(dev.volume, 50);
    EXPECT_FALSE(dev.muted);
    EXPECT_TRUE(dev.is_default);
    EXPECT_EQ(dev.type, AudioDeviceType::Sink);

    // Invoke sink input callback with a stream
    pa_sink_input_info stream{};
    stream.index = 12;
    stream.name = "Music App";
    stream.sink = 5;
    stream.volume.channels = 2;
    stream.volume.values[0] = PA_VOLUME_NORM;  // 100%
    stream.volume.values[1] = PA_VOLUME_NORM;
    stream.mute = 1;
    stream.proplist = pa_proplist_new();
    pa_proplist_sets(stream.proplist, PA_PROP_APPLICATION_NAME, "Spotify");
    pa_proplist_sets(stream.proplist, PA_PROP_APPLICATION_ICON_NAME, "spotify-icon");

    ASSERT_NE(mock_sys.sink_input_cb, nullptr);
    mock_sys.sink_input_cb(mock_sys.mock_context, &stream, 0, mock_sys.sink_input_userdata);

    QCoreApplication::processEvents();

    ASSERT_EQ(stream_added_spy.count(), 1);
    auto ast = stream_added_spy.first().at(0).value<AudioStream>();
    EXPECT_EQ(ast.id, 12U);
    EXPECT_EQ(ast.name, QStringLiteral("Music App"));
    EXPECT_EQ(ast.application, QStringLiteral("Spotify"));
    EXPECT_EQ(ast.icon_name, QStringLiteral("spotify-icon"));
    EXPECT_EQ(ast.volume, 100);
    EXPECT_TRUE(ast.muted);
    EXPECT_EQ(ast.type, AudioStreamType::SinkInput);

    pa_proplist_free(stream.proplist);
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, ProcessesSubscriptionChangeEvents) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    QSignalSpy sink_removed_spy(&backend, &PulseAudioBackend::sinkRemoved);
    QSignalSpy source_removed_spy(&backend, &PulseAudioBackend::sourceRemoved);
    QSignalSpy device_changed_spy(&backend, &PulseAudioBackend::deviceChanged);

    backend.start();
    mock_sys.context_state = PA_CONTEXT_READY;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);

    ASSERT_NE(mock_sys.subscribe_cb, nullptr);

    // Test SINK REMOVE event
    auto type = static_cast<pa_subscription_event_type_t>(PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_REMOVE);
    mock_sys.subscribe_cb(mock_sys.mock_context, type, 42, mock_sys.subscribe_userdata);

    QCoreApplication::processEvents();
    ASSERT_EQ(sink_removed_spy.count(), 1);
    EXPECT_EQ(sink_removed_spy.first().at(0).toUInt(), 42U);
    EXPECT_EQ(source_removed_spy.count(), 0);

    // Test SINK CHANGE event
    type = static_cast<pa_subscription_event_type_t>(PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_CHANGE);
    mock_sys.subscribe_cb(mock_sys.mock_context, type, 99, mock_sys.subscribe_userdata);

    ASSERT_NE(mock_sys.sink_by_idx_cb, nullptr);

    pa_sink_info sink{};
    sink.index = 99;
    sink.name = "changed-sink";
    sink.description = "Changed Sink Device";
    sink.volume.channels = 2;
    sink.volume.values[0] = PA_VOLUME_NORM * 0.75;
    sink.volume.values[1] = PA_VOLUME_NORM * 0.75;
    sink.mute = 1;

    mock_sys.sink_by_idx_cb(mock_sys.mock_context, &sink, 0, mock_sys.sink_by_idx_userdata);

    QCoreApplication::processEvents();
    ASSERT_EQ(device_changed_spy.count(), 1);
    auto dev = device_changed_spy.first().at(0).value<AudioDevice>();
    EXPECT_EQ(dev.id, 99U);
    EXPECT_EQ(dev.name, QStringLiteral("changed-sink"));
    EXPECT_TRUE(dev.muted);

    // Test SOURCE REMOVE event — must not cross-contaminate sinkRemoved.
    type = static_cast<pa_subscription_event_type_t>(  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        PA_SUBSCRIPTION_EVENT_SOURCE | PA_SUBSCRIPTION_EVENT_REMOVE);
    mock_sys.subscribe_cb(mock_sys.mock_context, type, 99, mock_sys.subscribe_userdata);

    QCoreApplication::processEvents();
    ASSERT_EQ(source_removed_spy.count(), 1);
    EXPECT_EQ(source_removed_spy.first().at(0).toUInt(), 99U);
    EXPECT_EQ(sink_removed_spy.count(), 1);  // unchanged from the earlier SINK removal
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, SetControlsTriggersCorrectPulseCalls) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);

  {
    PulseAudioBackend backend;
    backend.start();
    mock_sys.context_state = PA_CONTEXT_READY;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);

    backend.setDeviceVolume(5, 75);
    ASSERT_NE(mock_sys.sink_by_idx_cb, nullptr);
    pa_sink_info sink{};
    sink.index = 5;
    sink.volume.channels = 6;
    mock_sys.sink_by_idx_cb(mock_sys.mock_context, &sink, 0, mock_sys.sink_by_idx_userdata);
    mock_sys.sink_by_idx_cb(mock_sys.mock_context, nullptr, 1, mock_sys.sink_by_idx_userdata);
    EXPECT_EQ(mock_sys.last_sink_idx, 5U);
    EXPECT_EQ(mock_sys.last_sink_volume, 75);
    EXPECT_EQ(mock_sys.last_sink_channels, 6U);

    backend.setSourceVolume(7, 55);
    ASSERT_NE(mock_sys.source_by_idx_cb, nullptr);
    pa_source_info source{};
    source.index = 7;
    source.volume.channels = 1;
    mock_sys.source_by_idx_cb(mock_sys.mock_context, &source, 0, mock_sys.source_by_idx_userdata);
    mock_sys.source_by_idx_cb(mock_sys.mock_context, nullptr, 1, mock_sys.source_by_idx_userdata);
    EXPECT_EQ(mock_sys.last_source_idx, 7U);
    EXPECT_EQ(mock_sys.last_source_volume, 55);
    EXPECT_EQ(mock_sys.last_source_channels, 1U);

    backend.setDeviceVolume(6, 35);
    ASSERT_NE(mock_sys.sink_by_idx_cb, nullptr);
    pa_sink_info malformed_sink{};
    malformed_sink.index = 6;
    malformed_sink.volume.channels = 0;
    mock_sys.sink_by_idx_cb(mock_sys.mock_context, &malformed_sink, 0, mock_sys.sink_by_idx_userdata);
    mock_sys.sink_by_idx_cb(mock_sys.mock_context, nullptr, 1, mock_sys.sink_by_idx_userdata);
    EXPECT_EQ(mock_sys.last_sink_idx, 6U);
    EXPECT_EQ(mock_sys.last_sink_volume, 35);
    EXPECT_EQ(mock_sys.last_sink_channels, 1U);

    backend.setDeviceMuted(5, true);
    EXPECT_EQ(mock_sys.last_sink_idx, 5U);
    EXPECT_TRUE(mock_sys.last_sink_muted);

    backend.setDefaultOutput(3);
    EXPECT_EQ(mock_sys.last_default_sink, QStringLiteral("3"));

    backend.setDefaultInput(7);
    EXPECT_EQ(mock_sys.last_default_source, QStringLiteral("7"));

    backend.setStreamVolume(15, 90);
    ASSERT_NE(mock_sys.sink_input_by_idx_cb, nullptr);
    pa_sink_input_info stream{};
    stream.index = 15;
    stream.volume.channels = 8;
    mock_sys.sink_input_by_idx_cb(mock_sys.mock_context, &stream, 0, mock_sys.sink_input_by_idx_userdata);
    mock_sys.sink_input_by_idx_cb(mock_sys.mock_context, nullptr, 1, mock_sys.sink_input_by_idx_userdata);
    EXPECT_EQ(mock_sys.last_stream_idx, 15U);
    EXPECT_EQ(mock_sys.last_stream_volume, 90);
    EXPECT_EQ(mock_sys.last_stream_channels, 8U);

    backend.moveStreamToDevice(15, 2);
    EXPECT_EQ(mock_sys.last_move_stream_idx, 15U);
    EXPECT_EQ(mock_sys.last_move_device_idx, 2U);
  }

  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend,
     ReconnectsWithExponentialBackoffAndStopsAtCeiling) {  // NOLINT(readability-function-cognitive-complexity)
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);
  const std::vector<int> schedule = {10, 20, 40, 80, 160, 300};
  PulseAudioBackend::setReconnectBackoffScheduleForTests(schedule);

  {
    PulseAudioBackend backend;
    QSignalSpy health_spy(&backend, &PulseAudioBackend::healthStateChanged);

    backend.start();
    ASSERT_NE(mock_sys.state_cb, nullptr);

    // kMaxReconnectAttempts is 8 (private, mirrored here as a literal per DESIGN.md/TASKS.md).
    constexpr int kMaxReconnectAttempts = 8;
    // Delay used at each of the 8 scheduled attempts: doubling then held at the schedule's cap.
    const std::vector<int> expected_delays_ms = {10, 20, 40, 80, 160, 300, 300, 300};
    ASSERT_EQ(expected_delays_ms.size(), static_cast<size_t>(kMaxReconnectAttempts));

    mock_sys.context_state = PA_CONTEXT_FAILED;
    int connects_before = mock_sys.connect_calls;

    for (int attempt = 0; attempt < kMaxReconnectAttempts; ++attempt) {
      mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);
      QCoreApplication::processEvents();

      // Reconnect must not fire before the backoff delay elapses.
      EXPECT_EQ(mock_sys.connect_calls, connects_before) << "attempt " << attempt;

      QTest::qWait(expected_delays_ms[static_cast<size_t>(attempt)] + 150);
      EXPECT_EQ(mock_sys.connect_calls, connects_before + 1) << "attempt " << attempt;
      connects_before = mock_sys.connect_calls;
    }

    // healthStateChanged(Reconnecting) fired once (state stays Reconnecting across retries).
    int reconnecting_count = 0;
    int failed_count = 0;
    for (int i = 0; i < health_spy.count(); ++i) {
      const auto state = health_spy.at(i).at(0).value<AudioHealthState>();
      if (state == AudioHealthState::Reconnecting) {
        reconnecting_count++;
      } else if (state == AudioHealthState::Failed) {
        failed_count++;
      }
    }
    EXPECT_EQ(reconnecting_count, 1);
    EXPECT_EQ(failed_count, 0);

    // The 9th failure (after the 8th scheduled attempt also fails) hits the ceiling: no further
    // reconnect is scheduled, and healthStateChanged(Failed) fires exactly once.
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);
    QCoreApplication::processEvents();
    QTest::qWait(400);
    EXPECT_EQ(mock_sys.connect_calls, connects_before);

    failed_count = 0;
    for (int i = 0; i < health_spy.count(); ++i) {
      if (health_spy.at(i).at(0).value<AudioHealthState>() == AudioHealthState::Failed) {
        failed_count++;
      }
    }
    EXPECT_EQ(failed_count, 1);
  }

  PulseAudioBackend::resetReconnectBackoffSchedule();
  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, ReconnectSucceedsAfterTransientFailureAndPreservesExistingApi) {
  FakePulseAudioSystem mock_sys;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);
  PulseAudioBackend::setReconnectBackoffScheduleForTests({10, 20, 40, 80, 160, 300});

  {
    PulseAudioBackend backend;
    QSignalSpy health_spy(&backend, &PulseAudioBackend::healthStateChanged);
    QSignalSpy available_spy(&backend, &PulseAudioBackend::availableChanged);

    backend.start();
    ASSERT_NE(mock_sys.state_cb, nullptr);
    const int connects_before = mock_sys.connect_calls;

    // First connect attempt fails.
    mock_sys.context_state = PA_CONTEXT_FAILED;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);
    QCoreApplication::processEvents();

    // Wait for the scheduled reconnect to fire (a fresh context is created/connected).
    QTest::qWait(160);
    EXPECT_EQ(mock_sys.connect_calls, connects_before + 1);

    // The second attempt succeeds.
    mock_sys.context_state = PA_CONTEXT_READY;
    mock_sys.state_cb(mock_sys.mock_context, mock_sys.state_userdata);
    QCoreApplication::processEvents();

    ASSERT_GE(health_spy.count(), 2);
    EXPECT_EQ(health_spy.first().at(0).value<AudioHealthState>(), AudioHealthState::Reconnecting);
    EXPECT_EQ(health_spy.last().at(0).value<AudioHealthState>(), AudioHealthState::Connected);

    ASSERT_GE(available_spy.count(), 2);
    EXPECT_FALSE(available_spy.at(available_spy.count() - 2).at(0).toBool());
    EXPECT_TRUE(available_spy.last().at(0).toBool());

    // Existing control-plane calls still work identically after recovery — no stale state left
    // behind by the reconnect cycle (REQ-NF-001: existing audio-API signatures unchanged).
    backend.setDeviceVolume(5, 60);
    ASSERT_NE(mock_sys.sink_by_idx_cb, nullptr);
    pa_sink_info sink{};
    sink.index = 5;
    sink.volume.channels = 2;
    mock_sys.sink_by_idx_cb(mock_sys.mock_context, &sink, 0, mock_sys.sink_by_idx_userdata);
    mock_sys.sink_by_idx_cb(mock_sys.mock_context, nullptr, 1, mock_sys.sink_by_idx_userdata);
    EXPECT_EQ(mock_sys.last_sink_idx, 5U);
    EXPECT_EQ(mock_sys.last_sink_volume, 60);
  }

  PulseAudioBackend::resetReconnectBackoffSchedule();
  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, ImmediateConnectFailuresRetryAndStopMainloopSafely) {
  FakePulseAudioSystem mock_sys;
  mock_sys.context_to_return = nullptr;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);
  PulseAudioBackend::setReconnectBackoffScheduleForTests({1});

  {
    PulseAudioBackend backend;
    QSignalSpy health_spy(&backend, &PulseAudioBackend::healthStateChanged);
    backend.start();

    QTRY_VERIFY_WITH_TIMEOUT(health_spy.count() >= 2, 1000);
    EXPECT_EQ(health_spy.first().at(0).value<AudioHealthState>(), AudioHealthState::Reconnecting);
    EXPECT_EQ(health_spy.last().at(0).value<AudioHealthState>(), AudioHealthState::Failed);
    EXPECT_EQ(mock_sys.connect_calls, 0);
  }

  EXPECT_EQ(mock_sys.stop_calls, 1);
  PulseAudioBackend::resetReconnectBackoffSchedule();
  PulseAudioBackend::resetPulseAudioSystem();
}

TEST(PulseAudioBackend, SynchronousConnectFailuresReachRetryCeiling) {
  FakePulseAudioSystem mock_sys;
  mock_sys.connect_result = -1;
  PulseAudioBackend::setPulseAudioSystem(&mock_sys);
  PulseAudioBackend::setReconnectBackoffScheduleForTests({1});

  {
    PulseAudioBackend backend;
    QSignalSpy health_spy(&backend, &PulseAudioBackend::healthStateChanged);
    backend.start();

    QTRY_VERIFY_WITH_TIMEOUT(health_spy.count() >= 2, 1000);
    EXPECT_EQ(health_spy.last().at(0).value<AudioHealthState>(), AudioHealthState::Failed);
    EXPECT_EQ(mock_sys.connect_calls, 9);  // Initial attempt plus the bounded eight retries.
  }

  PulseAudioBackend::resetReconnectBackoffSchedule();
  PulseAudioBackend::resetPulseAudioSystem();
}
