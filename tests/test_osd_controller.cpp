#include "AudioChannelSource.h"
#include "AudioService.h"
#include "BrightnessBackend.h"
#include "BrightnessChannelSource.h"
#include "BrightnessService.h"
#include "HyprlandIpcClient.h"
#include "KeyboardLayoutChannelSource.h"
#include "KeyboardLayoutService.h"
#include "NullBrightnessBackend.h"
#include "OsdChannelSource.h"
#include "OsdController.h"
#include "OsdEvent.h"

#include <QMetaType>
#include <QSignalSpy>
#include <QVariant>

#include <chrono>
#include <gtest/gtest.h>
#include <holonight_shell_config/config_structs.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

using namespace HoloNight::ShellConfig;

// A source with no service behind it: tests drive it directly. Shared by every OsdController
// test below, so the controller is exercised without PulseAudio, sysfs, or Hyprland IPC.
class FakeChannelSource : public OsdChannelSource {
  Q_OBJECT

 public:
  explicit FakeChannelSource(QString channel_name, bool available = true, QObject* parent = nullptr)
      : OsdChannelSource(parent), channel_name_(std::move(channel_name)), available_(available) {}

  [[nodiscard]] QString channel() const override { return channel_name_; }
  [[nodiscard]] bool isAvailable() const override { return available_; }

  void emitLevel(int value, bool muted = false) {
    emit eventObserved(OsdLevelEvent{.channel = channel_name_, .value = value, .muted = muted});
  }

  void emitSelection(const QString& short_label, const QString& full_label) {
    emit eventObserved(
        OsdSelectionEvent{.channel = channel_name_, .short_label = short_label, .full_label = full_label});
  }

  void setAvailable(bool available) {
    if (available_ == available) {
      return;
    }
    available_ = available;
    emit availableChanged(available_);
  }

 private:
  QString channel_name_;
  bool available_;
};

// ---------------------------------------------------------------------------
// T-004 / REQ-F-002: the channel-source interface
// ---------------------------------------------------------------------------

TEST(OsdChannelSourceTest, FakeSubclassReportsChannelAndAvailability) {
  FakeChannelSource source(QStringLiteral("audio-volume"));
  EXPECT_EQ(source.channel(), QStringLiteral("audio-volume"));
  EXPECT_TRUE(source.isAvailable());

  FakeChannelSource unavailable(QStringLiteral("screen-brightness"), false);
  EXPECT_FALSE(unavailable.isAvailable());
}

TEST(OsdChannelSourceTest, SubclassEmitsBothBaseSignalsOnDemand) {
  FakeChannelSource source(QStringLiteral("audio-volume"));
  QSignalSpy event_spy(&source, &OsdChannelSource::eventObserved);
  QSignalSpy available_spy(&source, &OsdChannelSource::availableChanged);

  source.emitLevel(55, /*muted=*/true);
  ASSERT_EQ(event_spy.count(), 1);
  const auto level = std::get<OsdLevelEvent>(event_spy.at(0).at(0).value<OsdEvent>());
  EXPECT_EQ(level.channel, QStringLiteral("audio-volume"));
  EXPECT_EQ(level.value, 55);
  EXPECT_TRUE(level.muted);

  source.emitSelection(QStringLiteral("EN"), QStringLiteral("English (US)"));
  ASSERT_EQ(event_spy.count(), 2);
  const auto selection = std::get<OsdSelectionEvent>(event_spy.at(1).at(0).value<OsdEvent>());
  EXPECT_EQ(selection.short_label, QStringLiteral("EN"));
  EXPECT_EQ(selection.full_label, QStringLiteral("English (US)"));

  source.setAvailable(false);
  ASSERT_EQ(available_spy.count(), 1);
  EXPECT_FALSE(available_spy.at(0).at(0).toBool());
  EXPECT_FALSE(source.isAvailable());
}

TEST(OsdChannelSourceTest, BaseSignalsAreConnectableThroughTheBasePointer) {
  // The controller only ever holds OsdChannelSource*; connecting through that pointer must
  // reach a subclass's emissions.
  FakeChannelSource source(QStringLiteral("keyboard-layout"));
  OsdChannelSource* base = &source;

  int received = 0;
  QObject::connect(base, &OsdChannelSource::eventObserved, base, [&received](const OsdEvent&) { ++received; });
  source.emitSelection(QStringLiteral("DE"), QStringLiteral("German"));

  EXPECT_EQ(received, 1);
}

// ---------------------------------------------------------------------------
// T-003 / REQ-F-001: the normalized event value types
// ---------------------------------------------------------------------------

TEST(OsdEventTest, LevelEventHoldsChannelValueAndMuteState) {
  OsdLevelEvent event;
  EXPECT_TRUE(event.channel.isEmpty());
  EXPECT_EQ(event.value, 0);
  EXPECT_FALSE(event.muted);

  event.channel = QStringLiteral("audio-volume");
  event.value = 72;
  event.muted = true;

  EXPECT_EQ(event.channel, QStringLiteral("audio-volume"));
  EXPECT_EQ(event.value, 72);
  EXPECT_TRUE(event.muted);
}

TEST(OsdEventTest, SelectionEventHoldsChannelAndBothLabels) {
  OsdSelectionEvent event;
  EXPECT_TRUE(event.channel.isEmpty());
  EXPECT_TRUE(event.short_label.isEmpty());
  EXPECT_TRUE(event.full_label.isEmpty());

  event.channel = QStringLiteral("keyboard-layout");
  event.short_label = QStringLiteral("EN");
  event.full_label = QStringLiteral("English (US)");

  EXPECT_EQ(event.channel, QStringLiteral("keyboard-layout"));
  EXPECT_EQ(event.short_label, QStringLiteral("EN"));
  EXPECT_EQ(event.full_label, QStringLiteral("English (US)"));
}

TEST(OsdEventTest, BothEventsAreCopyableAndAssignable) {
  static_assert(std::is_copy_constructible_v<OsdLevelEvent>);
  static_assert(std::is_copy_assignable_v<OsdLevelEvent>);
  static_assert(std::is_copy_constructible_v<OsdSelectionEvent>);
  static_assert(std::is_copy_assignable_v<OsdSelectionEvent>);
  static_assert(std::is_copy_constructible_v<OsdEvent>);
  static_assert(std::is_copy_assignable_v<OsdEvent>);

  const OsdLevelEvent level{.channel = QStringLiteral("screen-brightness"), .value = 40, .muted = false};
  // The copy is the point of the test, not an oversight.
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const OsdLevelEvent level_copy = level;
  OsdLevelEvent level_assigned;
  level_assigned = level;
  EXPECT_EQ(level_copy, level);
  EXPECT_EQ(level_assigned, level);

  const OsdSelectionEvent selection{.channel = QStringLiteral("keyboard-layout"),
                                    .short_label = QStringLiteral("UK"),
                                    .full_label = QStringLiteral("Ukrainian")};
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const OsdSelectionEvent selection_copy = selection;
  OsdSelectionEvent selection_assigned;
  selection_assigned = selection;
  EXPECT_EQ(selection_copy, selection);
  EXPECT_EQ(selection_assigned, selection);
}

TEST(OsdEventTest, DefaultedEqualityComparesEveryMember) {
  const OsdLevelEvent base{.channel = QStringLiteral("audio-volume"), .value = 50, .muted = false};
  EXPECT_EQ(base, (OsdLevelEvent{.channel = QStringLiteral("audio-volume"), .value = 50, .muted = false}));
  EXPECT_NE(base, (OsdLevelEvent{.channel = QStringLiteral("screen-brightness"), .value = 50, .muted = false}));
  EXPECT_NE(base, (OsdLevelEvent{.channel = QStringLiteral("audio-volume"), .value = 51, .muted = false}));
  EXPECT_NE(base, (OsdLevelEvent{.channel = QStringLiteral("audio-volume"), .value = 50, .muted = true}));

  const OsdSelectionEvent selection{.channel = QStringLiteral("keyboard-layout"),
                                    .short_label = QStringLiteral("EN"),
                                    .full_label = QStringLiteral("English (US)")};
  EXPECT_EQ(selection, (OsdSelectionEvent{.channel = QStringLiteral("keyboard-layout"),
                                          .short_label = QStringLiteral("EN"),
                                          .full_label = QStringLiteral("English (US)")}));
  // full_label participates in operator==; the controller's diff rule deliberately does not.
  EXPECT_NE(selection, (OsdSelectionEvent{.channel = QStringLiteral("keyboard-layout"),
                                          .short_label = QStringLiteral("EN"),
                                          .full_label = QStringLiteral("English (UK)")}));
}

TEST(OsdEventTest, VariantAlternativesAreDistinguishable) {
  const OsdEvent level = OsdLevelEvent{.channel = QStringLiteral("audio-volume"), .value = 10, .muted = false};
  const OsdEvent selection = OsdSelectionEvent{.channel = QStringLiteral("keyboard-layout"),
                                               .short_label = QStringLiteral("DE"),
                                               .full_label = QStringLiteral("German")};

  EXPECT_TRUE(std::holds_alternative<OsdLevelEvent>(level));
  EXPECT_FALSE(std::holds_alternative<OsdSelectionEvent>(level));
  EXPECT_TRUE(std::holds_alternative<OsdSelectionEvent>(selection));
  EXPECT_EQ(std::get<OsdLevelEvent>(level).value, 10);
  EXPECT_EQ(std::get<OsdSelectionEvent>(selection).short_label, QStringLiteral("DE"));
}

TEST(OsdEventTest, AllThreeTypesRoundTripThroughQVariant) {
  // Q_DECLARE_METATYPE on each type is what makes this compile and round-trip; without it
  // the events could not cross a queued signal connection (REQ-NF-008).
  const OsdLevelEvent level{.channel = QStringLiteral("audio-volume"), .value = 33, .muted = true};
  const QVariant level_variant = QVariant::fromValue(level);
  ASSERT_TRUE(level_variant.canConvert<OsdLevelEvent>());
  EXPECT_EQ(level_variant.value<OsdLevelEvent>(), level);

  const OsdSelectionEvent selection{.channel = QStringLiteral("keyboard-layout"),
                                    .short_label = QStringLiteral("EN"),
                                    .full_label = QStringLiteral("English (US)")};
  const QVariant selection_variant = QVariant::fromValue(selection);
  ASSERT_TRUE(selection_variant.canConvert<OsdSelectionEvent>());
  EXPECT_EQ(selection_variant.value<OsdSelectionEvent>(), selection);

  const OsdEvent event = selection;
  const QVariant event_variant = QVariant::fromValue(event);
  ASSERT_TRUE(event_variant.canConvert<OsdEvent>());
  EXPECT_EQ(std::get<OsdSelectionEvent>(event_variant.value<OsdEvent>()), selection);
}

TEST(OsdEventTest, GadgetPropertiesExposeCamelCaseNamesToQml) {
  // QML reads `event.shortLabel`, not `event.short_label` — the Q_PROPERTY aliases are the
  // QML-facing contract and must survive any future member rename.
  const QMetaObject& level_meta = OsdLevelEvent::staticMetaObject;
  EXPECT_GE(level_meta.indexOfProperty("channel"), 0);
  EXPECT_GE(level_meta.indexOfProperty("value"), 0);
  EXPECT_GE(level_meta.indexOfProperty("muted"), 0);

  const QMetaObject& selection_meta = OsdSelectionEvent::staticMetaObject;
  EXPECT_GE(selection_meta.indexOfProperty("channel"), 0);
  EXPECT_GE(selection_meta.indexOfProperty("shortLabel"), 0);
  EXPECT_GE(selection_meta.indexOfProperty("fullLabel"), 0);
  EXPECT_EQ(selection_meta.indexOfProperty("short_label"), -1);
}

// ---------------------------------------------------------------------------
// T-005 / REQ-F-009, REQ-F-010: the AudioService and BrightnessService adapters
// ---------------------------------------------------------------------------

namespace {

// Mirrors the fake in test_brightness_service.cpp: BrightnessService takes ownership of a backend,
// which is the only way to give it a non-zero maxBrightness without a real /sys/class/backlight.
class FakeBrightnessBackend final : public BrightnessBackend {
 public:
  FakeBrightnessBackend(int max_brightness, int current_brightness, QObject* parent = nullptr)
      : BrightnessBackend(parent), max_brightness_(max_brightness), current_brightness_(current_brightness) {}

  [[nodiscard]] int maxBrightness() const override { return max_brightness_; }
  [[nodiscard]] int currentBrightness() const override { return current_brightness_; }
  void setBrightness(int value) override { current_brightness_ = value; }

  void triggerExternalChange(int new_raw) {
    current_brightness_ = new_raw;
    emit brightnessChanged(new_raw);
  }

 private:
  int max_brightness_;
  int current_brightness_;
};

// Unwraps the level event carried by an eventObserved emission recorded at `index`.
OsdLevelEvent levelAt(const QSignalSpy& spy, int index) {
  return std::get<OsdLevelEvent>(spy.at(index).at(0).value<OsdEvent>());
}

}  // namespace

TEST(AudioChannelSourceTest, ReportsItsChannelAndMirrorsServiceAvailability) {
  AudioService service(AudioService::SkipInit);
  AudioChannelSource source(&service);

  EXPECT_EQ(source.channel(), QStringLiteral("audio-volume"));
  EXPECT_FALSE(source.isAvailable());

  service.setAvailable(true);
  EXPECT_TRUE(source.isAvailable());
}

TEST(AudioChannelSourceTest, VolumeChangeEmitsLevelEvent) {
  AudioService service(AudioService::SkipInit);
  AudioChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  service.applyVolume(50);

  ASSERT_EQ(spy.count(), 1);
  const OsdLevelEvent event = levelAt(spy, 0);
  EXPECT_EQ(event.channel, QStringLiteral("audio-volume"));
  EXPECT_EQ(event.value, 50);
  EXPECT_FALSE(event.muted);
}

TEST(AudioChannelSourceTest, MuteChangeEmitsLevelEventCarryingTheCurrentVolume) {
  AudioService service(AudioService::SkipInit);
  service.applyVolume(50);
  AudioChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  // Mute is a separate service signal but not a separate OSD event: the emission carries the
  // volume too, so the renderer never has to merge a partial update.
  service.applyMuted(true);

  ASSERT_EQ(spy.count(), 1);
  const OsdLevelEvent event = levelAt(spy, 0);
  EXPECT_EQ(event.value, 50);
  EXPECT_TRUE(event.muted);
}

TEST(AudioChannelSourceTest, ConsecutiveVolumeChangesEmitSeparateEvents) {
  AudioService service(AudioService::SkipInit);
  AudioChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  service.applyVolume(50);
  service.applyVolume(75);

  ASSERT_EQ(spy.count(), 2);
  EXPECT_EQ(levelAt(spy, 0).value, 50);
  EXPECT_EQ(levelAt(spy, 1).value, 75);
}

TEST(AudioChannelSourceTest, ServiceAvailabilityIsForwardedAsABoolArgument) {
  // AudioService::availableChanged() carries no argument; the adapter has to re-read the property
  // to satisfy the base class's availableChanged(bool).
  AudioService service(AudioService::SkipInit);
  AudioChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::availableChanged);

  service.setAvailable(true);
  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).toBool());

  service.setAvailable(false);
  ASSERT_EQ(spy.count(), 2);
  EXPECT_FALSE(spy.at(1).at(0).toBool());
}

TEST(BrightnessChannelSourceTest, ReportsItsChannelAndMirrorsHasBacklight) {
  BrightnessService with_backlight(std::make_unique<FakeBrightnessBackend>(100, 0));
  BrightnessChannelSource source(&with_backlight);
  EXPECT_EQ(source.channel(), QStringLiteral("screen-brightness"));
  EXPECT_TRUE(source.isAvailable());

  BrightnessService without_backlight(std::make_unique<NullBrightnessBackend>());
  BrightnessChannelSource unavailable(&without_backlight);
  EXPECT_FALSE(unavailable.isAvailable());
}

TEST(BrightnessChannelSourceTest, BrightnessChangeEmitsLevelEventWithMutedFalse) {
  auto backend = std::make_unique<FakeBrightnessBackend>(100, 0);
  auto* backend_ptr = backend.get();
  BrightnessService service(std::move(backend));
  BrightnessChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  backend_ptr->triggerExternalChange(75);

  ASSERT_EQ(spy.count(), 1);
  const OsdLevelEvent event = levelAt(spy, 0);
  EXPECT_EQ(event.channel, QStringLiteral("screen-brightness"));
  EXPECT_EQ(event.value, 75);
  EXPECT_FALSE(event.muted);
}

TEST(BrightnessChannelSourceTest, ConsecutiveBrightnessChangesEmitSeparateEvents) {
  auto backend = std::make_unique<FakeBrightnessBackend>(100, 0);
  auto* backend_ptr = backend.get();
  BrightnessService service(std::move(backend));
  BrightnessChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  backend_ptr->triggerExternalChange(75);
  backend_ptr->triggerExternalChange(80);

  ASSERT_EQ(spy.count(), 2);
  EXPECT_EQ(levelAt(spy, 0).value, 75);
  EXPECT_EQ(levelAt(spy, 1).value, 80);
}

TEST(BrightnessChannelSourceTest, NeverEmitsAvailableChanged) {
  // Intentional, not an omission: hasBacklight is a CONSTANT property with no notify signal, so
  // there is nothing to observe. OsdController seeds availability with a post-connect
  // isAvailable() call instead (DESIGN.md §4). If this ever starts firing, that seed call in the
  // controller has become redundant and the two should be reconciled.
  auto backend = std::make_unique<FakeBrightnessBackend>(100, 0);
  auto* backend_ptr = backend.get();
  BrightnessService service(std::move(backend));
  BrightnessChannelSource source(&service);
  QSignalSpy spy(&source, &OsdChannelSource::availableChanged);

  backend_ptr->triggerExternalChange(75);
  service.setBrightnessPercent(20);

  EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// T-007 / REQ-F-011: the KeyboardLayoutService adapter
// ---------------------------------------------------------------------------

namespace {

// KeyboardLayoutService has no direct setter -- layout state only enters through the Hyprland IPC
// transport, so driving it in a test means faking that transport. Deliberately minimal compared to
// the one in test_keyboard_layout_service.cpp: these tests only ever push event lines.
class FakeLayoutTransport final : public HyprlandIpcTransport {
 public:
  explicit FakeLayoutTransport(QObject* parent = nullptr) : HyprlandIpcTransport(parent) {}

  void connectEventStream() override {}
  bool runCommand(const QByteArray& /*command*/, CommandCompletePredicate /*predicate*/ = {}) override { return true; }
  [[nodiscard]] bool hasRunningCommand() const override { return false; }

  void fireLayout(const QByteArray& layout_name) { emit eventLineReceived("activelayout>>kbd," + layout_name); }
};

// Unwraps the selection event carried by an eventObserved emission recorded at `index`.
OsdSelectionEvent selectionAt(const QSignalSpy& spy, int index) {
  return std::get<OsdSelectionEvent>(spy.at(index).at(0).value<OsdEvent>());
}

}  // namespace

TEST(KeyboardLayoutChannelSourceTest, ReportsItsChannelAndIsAlwaysAvailable) {
  KeyboardLayoutService service(std::make_unique<FakeLayoutTransport>());
  KeyboardLayoutChannelSource source(&service);

  EXPECT_EQ(source.channel(), QStringLiteral("keyboard-layout"));
  EXPECT_TRUE(source.isAvailable());
}

// A layout switch moves both properties, so KeyboardLayoutService fires both signals and this
// unconditional adapter translates each one -- two events per switch, not one. The pair that
// matters is the last, and the controller's shortLabel-only diff drops the other; see the
// EmissionOrder test below for why that is safe rather than merely tolerable.
TEST(KeyboardLayoutChannelSourceTest, LayoutChangeEmitsSelectionEventWithBothLabels) {
  auto transport = std::make_unique<FakeLayoutTransport>();
  FakeLayoutTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  KeyboardLayoutChannelSource source(&service);
  service.start();

  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  fake->fireLayout("English (US)");

  ASSERT_EQ(spy.count(), 2);
  const OsdSelectionEvent event = selectionAt(spy, 1);
  EXPECT_EQ(event.channel, QStringLiteral("keyboard-layout"));
  EXPECT_EQ(event.short_label, QStringLiteral("EN"));
  EXPECT_EQ(event.full_label, QStringLiteral("English (US)"));
}

TEST(KeyboardLayoutChannelSourceTest, SwitchingLayoutEmitsTheUpdatedPair) {
  auto transport = std::make_unique<FakeLayoutTransport>();
  FakeLayoutTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  KeyboardLayoutChannelSource source(&service);
  service.start();

  fake->fireLayout("English (US)");

  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  fake->fireLayout("German");

  ASSERT_EQ(spy.count(), 2);
  const OsdSelectionEvent event = selectionAt(spy, 1);
  EXPECT_EQ(event.short_label, QStringLiteral("DE"));
  EXPECT_EQ(event.full_label, QStringLiteral("German"));
}

// The load-bearing ordering test. KeyboardLayoutService commits the name before the code, so the
// first of the two emissions carries the NEW name beside the OLD code -- a genuinely mismatched
// pair. That is deliberate: because its shortLabel is unchanged, the controller's shortLabel-only
// diff discards it, and the correct pair arrives second and wins.
//
// Reverse the commit order in setLayoutName and this inverts: the mismatched pair would arrive
// second carrying the new code, the controller would display it, and the OSD would read "DE /
// English (US)". Nothing else in the system would catch that, which is why it is asserted here.
TEST(KeyboardLayoutChannelSourceTest, EmissionOrderKeepsTheMismatchedPairDiffable) {
  auto transport = std::make_unique<FakeLayoutTransport>();
  FakeLayoutTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  KeyboardLayoutChannelSource source(&service);
  service.start();

  fake->fireLayout("English (US)");

  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  fake->fireLayout("German");

  ASSERT_EQ(spy.count(), 2);

  // First: new name, old code -- diffable, therefore discarded downstream.
  const OsdSelectionEvent transient = selectionAt(spy, 0);
  EXPECT_EQ(transient.short_label, QStringLiteral("EN"));
  EXPECT_EQ(transient.full_label, QStringLiteral("German"));

  // Second: the coherent pair, the one the controller acts on.
  const OsdSelectionEvent settled = selectionAt(spy, 1);
  EXPECT_EQ(settled.short_label, QStringLiteral("DE"));
  EXPECT_EQ(settled.full_label, QStringLiteral("German"));
}

// A name-only change (two names sharing one code) still produces an event here. That is intended:
// this adapter is an unconditional translator and OsdController owns the diff, which compares only
// shortLabel and will drop this one. REQ-F-011's "emits nothing" is a controller-level guarantee,
// covered by T-008/T-009 -- asserting it here would put diff policy in two places.
TEST(KeyboardLayoutChannelSourceTest, NameOnlyChangeStillEmitsAndLeavesTheDiffToTheController) {
  auto transport = std::make_unique<FakeLayoutTransport>();
  FakeLayoutTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  KeyboardLayoutChannelSource source(&service);
  service.start();

  fake->fireLayout("English (US)");

  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  fake->fireLayout("English (UK)");

  ASSERT_EQ(spy.count(), 1);
  const OsdSelectionEvent event = selectionAt(spy, 0);
  EXPECT_EQ(event.short_label, QStringLiteral("EN"));
  EXPECT_EQ(event.full_label, QStringLiteral("English (UK)"));
}

// REQ-F-011: fullLabel falls back to the code so the renderer never receives an empty second line.
TEST(KeyboardLayoutChannelSourceTest, EmptyNameFallsBackToTheCodeForFullLabel) {
  KeyboardLayoutService service(std::make_unique<FakeLayoutTransport>());
  KeyboardLayoutChannelSource source(&service);

  QSignalSpy spy(&source, &OsdChannelSource::eventObserved);

  // No layout has been observed yet, so both properties are empty -- the degenerate case the
  // fallback exists for.
  emit service.layoutCodeChanged();

  ASSERT_EQ(spy.count(), 1);
  const OsdSelectionEvent event = selectionAt(spy, 0);
  EXPECT_TRUE(event.short_label.isEmpty());
  EXPECT_EQ(event.full_label, event.short_label);
}

TEST(KeyboardLayoutChannelSourceTest, NeverEmitsAvailableChanged) {
  auto transport = std::make_unique<FakeLayoutTransport>();
  FakeLayoutTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  KeyboardLayoutChannelSource source(&service);
  service.start();

  QSignalSpy spy(&source, &OsdChannelSource::availableChanged);

  fake->fireLayout("English (US)");
  fake->fireLayout("German");

  EXPECT_EQ(spy.count(), 0);
}

TEST(KeyboardLayoutChannelSourceTest, NullServiceIsInertRatherThanACrash) {
  KeyboardLayoutChannelSource source(nullptr);

  EXPECT_EQ(source.channel(), QStringLiteral("keyboard-layout"));
  EXPECT_TRUE(source.isAvailable());
}

// ---------------------------------------------------------------------------
// T-008 / REQ-F-003, F-004, F-008, F-025: the controller's gating chain
// ---------------------------------------------------------------------------

namespace {

// The injected clock (REQ-F-025). The grace period is a comparison, never an independent firing,
// so it needs no timer -- only a value the test can move. Nothing here sleeps.
struct FakeClock {
  std::chrono::steady_clock::time_point now{std::chrono::steady_clock::now()};

  void advance(std::chrono::milliseconds delta) { now += delta; }
};

constexpr auto kPastGracePeriod = std::chrono::milliseconds(2001);

OsdLevelEvent displayedLevel(const QSignalSpy& spy, int index) { return spy.at(index).at(0).value<OsdLevelEvent>(); }

OsdSelectionEvent displayedSelection(const QSignalSpy& spy, int index) {
  return spy.at(index).at(0).value<OsdSelectionEvent>();
}

}  // namespace

TEST(OsdControllerTest, ConstructionEmitsNothingAndArmsNoTimer) {
  // REQ-NF-001/NF-010: wiring up sources must not produce a display or leave a timer running.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);
  QSignalSpy hide_spy(&controller, &OsdController::hideRequested);

  EXPECT_EQ(level_spy.count(), 0);
  EXPECT_EQ(hide_spy.count(), 0);
  EXPECT_FALSE(controller.isHideTimerActiveForTest());
  EXPECT_TRUE(controller.currentChannelForTest().isEmpty());
}

TEST(OsdControllerTest, FirstEventOnAChannelPrimesSilently) {
  // The shell learns the current volume the moment it connects, not because the user changed it.
  // Showing that would mean an OSD on every login (REQ-F-003).
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);

  EXPECT_EQ(level_spy.count(), 0);
  EXPECT_FALSE(controller.isHideTimerActiveForTest());
}

TEST(OsdControllerTest, ChangedValueAfterTheGracePeriodEmitsExactlyOne) {
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  audio.emitLevel(51);

  ASSERT_EQ(level_spy.count(), 1);
  const OsdLevelEvent event = displayedLevel(level_spy, 0);
  EXPECT_EQ(event.channel, QStringLiteral("audio-volume"));
  EXPECT_EQ(event.value, 51);
  EXPECT_EQ(controller.currentChannelForTest(), QStringLiteral("audio-volume"));
  EXPECT_TRUE(controller.isHideTimerActiveForTest());
}

TEST(OsdControllerTest, RepeatOfTheSameValueEmitsNothing) {
  // PulseAudio re-broadcasts unchanged sink state on unrelated events; without this the OSD would
  // flash for changes that never happened.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  audio.emitLevel(51);
  audio.emitLevel(51);

  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, ReturningToAPreviouslySeenValueEmitsAgain) {
  // The diff is against the last value, not a set of values ever seen -- REQ-F-003's fourth bullet.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime
  audio.emitLevel(51);
  audio.emitLevel(51);
  audio.emitLevel(50);

  ASSERT_EQ(level_spy.count(), 2);
  EXPECT_EQ(displayedLevel(level_spy, 0).value, 51);
  EXPECT_EQ(displayedLevel(level_spy, 1).value, 50);
}

TEST(OsdControllerTest, MuteToggleAtAnUnchangedVolumeCountsAsAChange) {
  // Muting does not move the percentage, so a value-only diff would swallow the one event the user
  // most expects to see.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50, /*muted=*/false);
  audio.emitLevel(50, /*muted=*/true);

  ASSERT_EQ(level_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).value, 50);
  EXPECT_TRUE(displayedLevel(level_spy, 0).muted);
}

// Closes the loop T-007 deliberately left open: the adapter emits on a name-only change, and this
// is the diff that drops it. Both halves of REQ-F-011 are now pinned, each where it belongs.
TEST(OsdControllerTest, SelectionDiffComparesShortLabelAndIgnoresFullLabel) {
  FakeClock fake_clock;
  FakeChannelSource layout(QStringLiteral("keyboard-layout"));
  OsdController controller({&layout}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy selection_spy(&controller, &OsdController::displaySelectionEvent);

  layout.emitSelection(QStringLiteral("EN"), QStringLiteral("English (US)"));  // prime
  layout.emitSelection(QStringLiteral("EN"), QStringLiteral("English (UK)"));  // same code, no OSD
  layout.emitSelection(QStringLiteral("DE"), QStringLiteral("German"));

  ASSERT_EQ(selection_spy.count(), 1);
  EXPECT_EQ(displayedSelection(selection_spy, 0).short_label, QStringLiteral("DE"));
  EXPECT_EQ(displayedSelection(selection_spy, 0).full_label, QStringLiteral("German"));
}

// The mismatched-pair sequence T-007 proved the adapter produces, run through the real controller:
// the transient {EN, "German"} must not reach the screen.
TEST(OsdControllerTest, TheTransientPairFromALayoutSwitchIsDiffedAway) {
  FakeClock fake_clock;
  FakeChannelSource layout(QStringLiteral("keyboard-layout"));
  OsdController controller({&layout}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy selection_spy(&controller, &OsdController::displaySelectionEvent);

  layout.emitSelection(QStringLiteral("EN"), QStringLiteral("English (US)"));  // prime
  layout.emitSelection(QStringLiteral("EN"), QStringLiteral("German"));        // name committed first
  layout.emitSelection(QStringLiteral("DE"), QStringLiteral("German"));        // code follows

  ASSERT_EQ(selection_spy.count(), 1);
  EXPECT_EQ(displayedSelection(selection_spy, 0).short_label, QStringLiteral("DE"));
  EXPECT_EQ(displayedSelection(selection_spy, 0).full_label, QStringLiteral("German"));
}

TEST(OsdControllerTest, EachKindDispatchesToItsOwnSignal) {
  // REQ-F-008: the variant alternative, not the channel name, picks the signal.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  FakeChannelSource layout(QStringLiteral("keyboard-layout"));
  OsdController controller({&audio, &layout}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);
  QSignalSpy selection_spy(&controller, &OsdController::displaySelectionEvent);

  audio.emitLevel(50);
  audio.emitLevel(60);
  layout.emitSelection(QStringLiteral("EN"), QStringLiteral("English (US)"));
  layout.emitSelection(QStringLiteral("DE"), QStringLiteral("German"));

  ASSERT_EQ(level_spy.count(), 1);
  ASSERT_EQ(selection_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).value, 60);
  EXPECT_EQ(displayedSelection(selection_spy, 0).short_label, QStringLiteral("DE"));
  EXPECT_EQ(controller.currentChannelForTest(), QStringLiteral("keyboard-layout"));
}

TEST(OsdControllerTest, ChannelsArePrimedIndependently) {
  // One channel's prime must not count as another's, or the second channel to report would show an
  // OSD for a value nobody changed.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  FakeChannelSource brightness(QStringLiteral("screen-brightness"));
  OsdController controller({&audio, &brightness}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  audio.emitLevel(60);
  brightness.emitLevel(60);  // same number, different channel -- still a prime

  ASSERT_EQ(level_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).channel, QStringLiteral("audio-volume"));
}

TEST(OsdControllerTest, EventsDuringTheGracePeriodEmitNothingAndAreNeverFlushed) {
  // REQ-F-004: services report their initial state asynchronously over the first couple of seconds,
  // so the window exists to swallow that burst. Discarded, not queued.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(40);
  audio.emitLevel(50);
  audio.emitLevel(60);
  EXPECT_EQ(level_spy.count(), 0);

  fake_clock.advance(kPastGracePeriod);

  EXPECT_EQ(level_spy.count(), 0);
  EXPECT_FALSE(controller.isHideTimerActiveForTest());
}

TEST(OsdControllerTest, AValueDiscardedDuringGraceStillUpdatesTheCache) {
  // The discard happens after the cache write, not instead of it. If it were the other way round,
  // the first post-grace event would diff against a pre-grace value and show a change the user
  // made two seconds ago (REQ-F-004's fourth bullet).
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime, inside grace
  audio.emitLevel(60);  // a real change, discarded by grace
  ASSERT_EQ(level_spy.count(), 0);

  fake_clock.advance(kPastGracePeriod);

  audio.emitLevel(60);  // still 60: nothing changed while we were not looking
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(70);
  ASSERT_EQ(level_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).value, 70);
}

TEST(OsdControllerTest, GracePeriodEndsExactlyAtTwoSeconds) {
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime

  fake_clock.advance(std::chrono::milliseconds(1999));
  audio.emitLevel(60);
  EXPECT_EQ(level_spy.count(), 0);

  fake_clock.advance(std::chrono::milliseconds(1));  // 2000 ms: the window is half-open, so this is out
  audio.emitLevel(70);
  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, LosingAvailabilityRePrimesTheChannel) {
  // An audio backend restart or a backlight vanishing on monitor hotplug restores a value that may
  // differ from the one cached before the outage. Diffing across that gap would flash an OSD for a
  // change that never happened.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime
  audio.setAvailable(false);

  audio.setAvailable(true);
  audio.emitLevel(80);  // re-prime, silent even though it differs from 50
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(85);
  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, NullSourcesAreSkippedRatherThanCrashing) {
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({nullptr, &audio, nullptr}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  audio.emitLevel(60);

  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, AnEmptyNowFnFallsBackToTheRealClock) {
  // The default argument is a lambda, but a caller can still pass a value-initialized NowFn. Calling
  // through an empty std::function would throw std::bad_function_call on the first event.
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, OsdController::NowFn{});

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  audio.emitLevel(60);

  // Real clock, so both land inside the 2000 ms grace window -- the assertion is that neither
  // call aborted.
  EXPECT_EQ(level_spy.count(), 0);
}

// ---------------------------------------------------------------------------
// T-009 / REQ-F-005, F-007, F-017, C-010, C-011, C-015: the rest of the gating
// matrix -- suppression, enable/disable, the timeout clamp, and the hide timer
// ---------------------------------------------------------------------------

TEST(OsdControllerTest, SuppressedChannelEmitsNothing) {
  // REQ-F-005. The controller never asks whether a surface is visible -- holonight_services may not
  // depend on holonight_surfaces -- so suppression only ever arrives through this setter.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime
  controller.setSuppressed(QStringLiteral("audio-volume"), true);
  audio.emitLevel(60);

  EXPECT_EQ(level_spy.count(), 0);
  EXPECT_FALSE(controller.isHideTimerActiveForTest());
}

TEST(OsdControllerTest, UnsuppressingDoesNotRetroactivelyFire) {
  // The central consequence of writing the cache ahead of every gate: suppression is a gate flip,
  // not a deferred queue. Closing the quick-settings panel must not replay the slider drag the user
  // just performed inside it (REQ-F-005).
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime
  controller.setSuppressed(QStringLiteral("audio-volume"), true);
  audio.emitLevel(60);
  audio.emitLevel(70);
  ASSERT_EQ(level_spy.count(), 0);

  controller.setSuppressed(QStringLiteral("audio-volume"), false);
  EXPECT_EQ(level_spy.count(), 0);

  // The cache tracked every suppressed value, so re-observing the last one is not a change.
  audio.emitLevel(70);
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(80);
  ASSERT_EQ(level_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).value, 80);
}

TEST(OsdControllerTest, SuppressionAppliesToOneChannelOnly) {
  // Suppression is keyed by channel because the surfaces that trigger it are channel-specific: the
  // audio popup hides volume OSDs, it has nothing to say about the keyboard layout.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  FakeChannelSource brightness(QStringLiteral("screen-brightness"));
  OsdController controller({&audio, &brightness}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  brightness.emitLevel(40);
  controller.setSuppressed(QStringLiteral("audio-volume"), true);

  audio.emitLevel(60);
  brightness.emitLevel(45);

  ASSERT_EQ(level_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).channel, QStringLiteral("screen-brightness"));
}

TEST(OsdControllerTest, MasterDisableBlocksEveryChannel) {
  // REQ-C-010. The spec phrases this as "constructs controller with enabled=false"; there is no such
  // constructor parameter by design -- config arrives through setters so a live reload uses the same
  // path as startup -- so calling it before any event is the equivalent.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  FakeChannelSource layout(QStringLiteral("keyboard-layout"));
  OsdController controller({&audio, &layout}, [&fake_clock] { return fake_clock.now; });
  controller.setEnabled(false);
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);
  QSignalSpy selection_spy(&controller, &OsdController::displaySelectionEvent);

  audio.emitLevel(50);
  audio.emitLevel(60);
  layout.emitSelection(QStringLiteral("EN"), QStringLiteral("English (US)"));
  layout.emitSelection(QStringLiteral("DE"), QStringLiteral("German"));

  EXPECT_EQ(level_spy.count(), 0);
  EXPECT_EQ(selection_spy.count(), 0);
  EXPECT_FALSE(controller.isHideTimerActiveForTest());
}

TEST(OsdControllerTest, ReEnablingDoesNotRetroactivelyFire) {
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime
  controller.setEnabled(false);
  audio.emitLevel(60);
  ASSERT_EQ(level_spy.count(), 0);

  controller.setEnabled(true);
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(60);  // unchanged since the disabled window: still nothing
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(70);
  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, PerChannelDisableBlocksOnlyItsOwnChannel) {
  // REQ-F-007: a user who wants brightness OSDs but not volume OSDs sets one flag, and the other
  // channels are untouched.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  FakeChannelSource brightness(QStringLiteral("screen-brightness"));
  OsdController controller({&audio, &brightness}, [&fake_clock] { return fake_clock.now; });
  controller.setChannelEnabled(QStringLiteral("audio-volume"), false);
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);
  brightness.emitLevel(40);
  audio.emitLevel(60);
  brightness.emitLevel(45);

  ASSERT_EQ(level_spy.count(), 1);
  EXPECT_EQ(displayedLevel(level_spy, 0).channel, QStringLiteral("screen-brightness"));
}

TEST(OsdControllerTest, ChannelsAreEnabledUntilExplicitlyDisabled) {
  // Absence in the hash means enabled. The config only ever writes the channels it knows about, and
  // a channel added later must not be silently mute until someone remembers to seed it.
  FakeClock fake_clock;
  FakeChannelSource unseeded(QStringLiteral("channel-nobody-configured"));
  OsdController controller({&unseeded}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  unseeded.emitLevel(50);
  unseeded.emitLevel(60);

  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, ReEnablingAChannelDoesNotRetroactivelyFire) {
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy level_spy(&controller, &OsdController::displayLevelEvent);

  audio.emitLevel(50);  // prime
  controller.setChannelEnabled(QStringLiteral("audio-volume"), false);
  audio.emitLevel(60);
  ASSERT_EQ(level_spy.count(), 0);

  controller.setChannelEnabled(QStringLiteral("audio-volume"), true);
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(60);
  EXPECT_EQ(level_spy.count(), 0);

  audio.emitLevel(70);
  EXPECT_EQ(level_spy.count(), 1);
}

TEST(OsdControllerTest, TimeoutBelowTheMinimumClampsUp) {
  // REQ-C-011. Clamping rather than rejecting: a nonsensical config value should degrade to the
  // nearest usable OSD, not silently disable the feature.
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio});

  controller.setTimeoutMs(100);

  EXPECT_EQ(controller.timeoutMsForTest(), OsdConfig::kMinTimeoutMs);
}

TEST(OsdControllerTest, TimeoutAboveTheMaximumClampsDown) {
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio});

  controller.setTimeoutMs(20000);

  EXPECT_EQ(controller.timeoutMsForTest(), OsdConfig::kMaxTimeoutMs);
}

TEST(OsdControllerTest, TimeoutInsideTheRangeIsStoredVerbatim) {
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio});

  controller.setTimeoutMs(2500);

  EXPECT_EQ(controller.timeoutMsForTest(), 2500);
}

TEST(OsdControllerTest, TimeoutBoundsThemselvesAreAccepted) {
  // The range is inclusive at both ends, so neither bound may be nudged inward by the clamp.
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio});

  controller.setTimeoutMs(OsdConfig::kMinTimeoutMs);
  EXPECT_EQ(controller.timeoutMsForTest(), OsdConfig::kMinTimeoutMs);

  controller.setTimeoutMs(OsdConfig::kMaxTimeoutMs);
  EXPECT_EQ(controller.timeoutMsForTest(), OsdConfig::kMaxTimeoutMs);
}

TEST(OsdControllerTest, TheDefaultTimeoutMatchesTheConfigDefault) {
  // Two independent defaults exist (the config struct's and the member initializer). They are only
  // equal by convention, and a shell that never applies config must still time out at the same
  // duration as one that does.
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio});

  EXPECT_EQ(controller.timeoutMsForTest(), OsdConfig{}.timeout_ms);
}

TEST(OsdControllerTest, HideRequestedFiresAfterTheTimeoutAndClearsTheCurrentChannel) {
  // The one place a real QTimer is unavoidable: the hide is an independent firing, not a comparison
  // the test can force by advancing a value. Uses the shortest legal timeout to stay quick.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  controller.setTimeoutMs(OsdConfig::kMinTimeoutMs);
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy hide_spy(&controller, &OsdController::hideRequested);

  audio.emitLevel(50);  // prime
  audio.emitLevel(60);  // displays
  ASSERT_TRUE(controller.isHideTimerActiveForTest());
  ASSERT_EQ(controller.currentChannelForTest(), QStringLiteral("audio-volume"));

  ASSERT_TRUE(hide_spy.wait(2000));
  EXPECT_EQ(hide_spy.count(), 1);
  EXPECT_TRUE(controller.currentChannelForTest().isEmpty());
}

TEST(OsdControllerTest, TheHideTimerRestartsRatherThanStacksOnSameChannelUpdates) {
  // REQ-F-017: holding volume-up must keep the OSD on screen for a full timeout after the *last*
  // step, not vanish a timeout after the first. A QTimer::start() on a running timer restarts it, so
  // the failure this guards against is anyone replacing it with a conditional start.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  controller.setTimeoutMs(OsdConfig::kMinTimeoutMs);  // 300 ms
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy hide_spy(&controller, &OsdController::hideRequested);

  audio.emitLevel(50);  // prime
  audio.emitLevel(60);  // displays; hide due at ~300 ms

  ASSERT_FALSE(hide_spy.wait(200));  // ~200 ms elapsed, still visible

  audio.emitLevel(70);  // restarts: hide now due at ~500 ms

  // Ends at ~400 ms. A timer that had not restarted would have fired at ~300 ms and been caught
  // here; a stacked second shot would fire here too.
  EXPECT_FALSE(hide_spy.wait(200));

  EXPECT_TRUE(hide_spy.wait(2000));
  EXPECT_EQ(hide_spy.count(), 1);
}

TEST(OsdControllerTest, ChangingTheTimeoutDoesNotExtendAnOsdAlreadyOnScreen) {
  // A config reload mid-display keeps the duration the display started with; the new value applies
  // from the next one. Otherwise editing the config file would freeze whatever is currently up.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  controller.setTimeoutMs(OsdConfig::kMinTimeoutMs);
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy hide_spy(&controller, &OsdController::hideRequested);

  audio.emitLevel(50);  // prime
  audio.emitLevel(60);  // displays with a 300 ms timeout

  controller.setTimeoutMs(OsdConfig::kMaxTimeoutMs);

  // Would need 10 seconds if the running timer had adopted the new value.
  EXPECT_TRUE(hide_spy.wait(2000));
  EXPECT_EQ(controller.timeoutMsForTest(), OsdConfig::kMaxTimeoutMs);
}

TEST(OsdControllerTest, DisablingMidDisplayLeavesTheRunningHideTimerToFinish) {
  // setEnabled(false) deliberately does not force a hide: the timer already running clears the
  // surface on schedule, and a forced hide would be the only path emitting hideRequested() without
  // a preceding display.
  FakeClock fake_clock;
  FakeChannelSource audio(QStringLiteral("audio-volume"));
  OsdController controller({&audio}, [&fake_clock] { return fake_clock.now; });
  controller.setTimeoutMs(OsdConfig::kMinTimeoutMs);
  fake_clock.advance(kPastGracePeriod);

  QSignalSpy hide_spy(&controller, &OsdController::hideRequested);

  audio.emitLevel(50);  // prime
  audio.emitLevel(60);  // displays
  controller.setEnabled(false);

  EXPECT_EQ(hide_spy.count(), 0);  // not hidden synchronously
  EXPECT_TRUE(hide_spy.wait(2000));
  EXPECT_EQ(hide_spy.count(), 1);
}

#include "test_osd_controller.moc"
