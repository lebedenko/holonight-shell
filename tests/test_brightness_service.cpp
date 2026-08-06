#include "BrightnessBackend.h"
#include "BrightnessService.h"
#include "NullBrightnessBackend.h"

#include <QSignalSpy>

#include <gtest/gtest.h>
#include <memory>

namespace {

class FakeBrightnessBackend final : public BrightnessBackend {
 public:
  explicit FakeBrightnessBackend(int max_brightness, int current_brightness, QObject* parent = nullptr)
      : BrightnessBackend(parent), max_brightness_(max_brightness), current_brightness_(current_brightness) {}

  [[nodiscard]] int maxBrightness() const override { return max_brightness_; }
  [[nodiscard]] int currentBrightness() const override { return current_brightness_; }
  void setBrightness(int value) override {
    last_set_brightness_ = value;
    ++set_brightness_calls_;
  }

  void triggerExternalChange(int new_raw) {
    current_brightness_ = new_raw;
    emit brightnessChanged(new_raw);
  }

  [[nodiscard]] int lastSetBrightness() const { return last_set_brightness_; }
  [[nodiscard]] int setBrightnessCalls() const { return set_brightness_calls_; }

 private:
  int last_set_brightness_{-1};
  int set_brightness_calls_{0};
  int max_brightness_;
  int current_brightness_;
};

}  // namespace

// ─── computePercent ──────────────────────────────────────────────────────────

TEST(BrightnessServiceTest, PercentAt0) {
  auto backend = std::make_unique<FakeBrightnessBackend>(4437, 0);
  BrightnessService service(std::move(backend));
  EXPECT_EQ(service.brightnessPercent(), 0);
}

TEST(BrightnessServiceTest, PercentAt100) {
  auto backend = std::make_unique<FakeBrightnessBackend>(4437, 4437);
  BrightnessService service(std::move(backend));
  EXPECT_EQ(service.brightnessPercent(), 100);
}

TEST(BrightnessServiceTest, PercentAt50) {
  auto backend = std::make_unique<FakeBrightnessBackend>(4437, 2218);
  BrightnessService service(std::move(backend));
  EXPECT_EQ(service.brightnessPercent(), 50);
}

TEST(BrightnessServiceTest, PercentClampsOutOfRangeInitialValue) {
  auto below_minimum = std::make_unique<FakeBrightnessBackend>(1000, -10);
  BrightnessService below_minimum_service(std::move(below_minimum));
  EXPECT_EQ(below_minimum_service.brightnessPercent(), 0);

  auto above_maximum = std::make_unique<FakeBrightnessBackend>(1000, 1010);
  BrightnessService above_maximum_service(std::move(above_maximum));
  EXPECT_EQ(above_maximum_service.brightnessPercent(), 100);
}

TEST(BrightnessServiceTest, NoDivisionByZeroWhenNoBacklight) {
  auto backend = std::make_unique<NullBrightnessBackend>();
  BrightnessService service(std::move(backend));
  EXPECT_EQ(service.brightnessPercent(), 0);
  EXPECT_FALSE(service.hasBacklight());
  EXPECT_EQ(service.maxBrightness(), 0);
}

// ─── setBrightnessPercent ────────────────────────────────────────────────────

TEST(BrightnessServiceTest, SetBrightnessPercentDelegatesToBackend) {
  auto* raw = new FakeBrightnessBackend(4437, 0);
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};
  service.setBrightnessPercent(50);
  EXPECT_EQ(raw->setBrightnessCalls(), 1);
  EXPECT_NEAR(raw->lastSetBrightness(), 2219, 2);
}

TEST(BrightnessServiceTest, SetBrightnessPercent100SetsMax) {
  auto* raw = new FakeBrightnessBackend(4437, 0);
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};
  service.setBrightnessPercent(100);
  EXPECT_EQ(raw->lastSetBrightness(), 4437);
}

TEST(BrightnessServiceTest, SetBrightnessPercentClampsOutOfRangeValues) {
  auto* raw = new FakeBrightnessBackend(1000, 0);
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};

  service.setBrightnessPercent(-20);
  EXPECT_EQ(raw->lastSetBrightness(), 0);

  service.setBrightnessPercent(140);
  EXPECT_EQ(raw->lastSetBrightness(), 1000);
}

TEST(BrightnessServiceTest, SetBrightnessPercentIsNoopWhenNoBacklight) {
  auto* raw = new NullBrightnessBackend();
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};
  service.setBrightnessPercent(50);
  EXPECT_EQ(raw->maxBrightness(), 0);
}

// ─── external change propagation ─────────────────────────────────────────────

TEST(BrightnessServiceTest, ExternalChangeEmitsBrightnessPercentChanged) {
  auto* raw = new FakeBrightnessBackend(4437, 0);
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};
  QSignalSpy spy(&service, &BrightnessService::brightnessPercentChanged);
  raw->triggerExternalChange(2218);
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(0).toInt(), 50);
}

TEST(BrightnessServiceTest, ExternalChangeClampsOutOfRangeValue) {
  auto* raw = new FakeBrightnessBackend(1000, 500);
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};
  QSignalSpy spy(&service, &BrightnessService::brightnessPercentChanged);

  raw->triggerExternalChange(1010);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(service.brightnessPercent(), 100);
  EXPECT_EQ(spy.at(0).at(0).toInt(), 100);

  raw->triggerExternalChange(-10);

  ASSERT_EQ(spy.count(), 2);
  EXPECT_EQ(service.brightnessPercent(), 0);
  EXPECT_EQ(spy.at(1).at(0).toInt(), 0);
}

TEST(BrightnessServiceTest, DuplicateExternalChangeDoesNotEmitSignal) {
  auto* raw = new FakeBrightnessBackend(4437, 2218);
  BrightnessService service{std::unique_ptr<BrightnessBackend>(raw)};
  QSignalSpy spy(&service, &BrightnessService::brightnessPercentChanged);
  raw->triggerExternalChange(2218);
  EXPECT_EQ(spy.count(), 0);
}
