#include "IdleBackend.h"
#include "IdleInhibitor.h"
#include "IdleService.h"
#include "ScreenSaverAdaptor.h"

#include <QSignalSpy>

#include <gtest/gtest.h>
#include <memory>

namespace {

// Controllable backend: tests call triggerIdle/triggerResume to drive
// idleThresholdExceeded and set the value returned by getSessionIdleTimeSeconds.
class FakeIdleBackend final : public IdleBackend {
 public:
  explicit FakeIdleBackend(QObject* parent = nullptr) : IdleBackend(parent) {}

  [[nodiscard]] uint getSessionIdleTimeSeconds() const override { return idle_secs_; }

  void triggerIdle() { emit idleThresholdExceeded(true); }
  void triggerResume() { emit idleThresholdExceeded(false); }
  void setIdleSecs(uint secs) { idle_secs_ = secs; }

 private:
  uint idle_secs_{0};
};

class FakeIdleInhibitor final : public IdleInhibitor {
 public:
  bool acquire(const QString& reason) override {
    ++acquire_calls;
    last_reason = reason;
    if (!acquire_result) {
      return false;
    }
    held = true;
    return true;
  }

  void release() override {
    ++release_calls;
    held = false;
  }

  bool acquire_result{true};
  bool held{false};
  int acquire_calls{0};
  int release_calls{0};
  QString last_reason;
};

}  // namespace

// ─── IdleService ─────────────────────────────────────────────────────────────

TEST(IdleServiceTest, StartsInNonIdleState) {
  auto backend = std::make_unique<FakeIdleBackend>();
  IdleService service(std::move(backend), /*daemon_detected=*/false);
  EXPECT_FALSE(service.isIdle());
}

TEST(IdleServiceTest, BecomeIdleWhenBackendSignals) {
  auto* raw = new FakeIdleBackend;
  IdleService service(std::unique_ptr<IdleBackend>(raw), false);
  raw->triggerIdle();
  EXPECT_TRUE(service.isIdle());
}

TEST(IdleServiceTest, EmitsIdleChangedOnThresholdCrossing) {
  auto* raw = new FakeIdleBackend;
  IdleService service(std::unique_ptr<IdleBackend>(raw), false);
  QSignalSpy spy(&service, &IdleService::idleChanged);

  raw->triggerIdle();

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).toBool());
}

TEST(IdleServiceTest, ResumesFromIdleAndEmitsSignal) {
  auto* raw = new FakeIdleBackend;
  IdleService service(std::unique_ptr<IdleBackend>(raw), false);
  QSignalSpy spy(&service, &IdleService::idleChanged);

  raw->triggerIdle();
  raw->triggerResume();

  ASSERT_EQ(spy.count(), 2);
  EXPECT_FALSE(spy.at(1).at(0).toBool());
  EXPECT_FALSE(service.isIdle());
}

TEST(IdleServiceTest, DuplicateThresholdEventDoesNotReemit) {
  auto* raw = new FakeIdleBackend;
  IdleService service(std::unique_ptr<IdleBackend>(raw), false);
  QSignalSpy spy(&service, &IdleService::idleChanged);

  raw->triggerIdle();
  raw->triggerIdle();  // second idle while already idle → no extra signal

  EXPECT_EQ(spy.count(), 1);
}

TEST(IdleServiceTest, GetIdleTimeSecondsDelegatesToBackend) {
  auto* raw = new FakeIdleBackend;
  raw->setIdleSecs(42);
  IdleService service(std::unique_ptr<IdleBackend>(raw), false);
  EXPECT_EQ(service.getIdleTimeSeconds(), 42U);
}

TEST(IdleServiceTest, DaemonDetectedReflectsInjectedValue) {
  auto backendTrue = std::make_unique<FakeIdleBackend>();
  IdleService serviceTrue(std::move(backendTrue), /*daemon_detected=*/true);
  EXPECT_TRUE(serviceTrue.idleDaemonDetected());

  auto backendFalse = std::make_unique<FakeIdleBackend>();
  IdleService serviceFalse(std::move(backendFalse), /*daemon_detected=*/false);
  EXPECT_FALSE(serviceFalse.idleDaemonDetected());
}

TEST(IdleServiceTest, DefaultThresholdIs300000) {
  auto backend = std::make_unique<FakeIdleBackend>();
  IdleService service(std::move(backend), false);
  EXPECT_EQ(service.idleThresholdMs(), 300'000);
}

TEST(IdleServiceTest, SetIdleThresholdMsUpdatesValueAndEmitsSignal) {
  auto backend = std::make_unique<FakeIdleBackend>();
  IdleService service(std::move(backend), false);
  QSignalSpy spy(&service, &IdleService::idleThresholdMsChanged);

  service.setIdleThresholdMs(60'000);

  EXPECT_EQ(service.idleThresholdMs(), 60'000);
  EXPECT_EQ(spy.count(), 1);
}

TEST(IdleServiceTest, SetIdleThresholdMsNoSignalWhenUnchanged) {
  auto backend = std::make_unique<FakeIdleBackend>();
  IdleService service(std::move(backend), false);
  QSignalSpy spy(&service, &IdleService::idleThresholdMsChanged);

  service.setIdleThresholdMs(service.idleThresholdMs());

  EXPECT_EQ(spy.count(), 0);
}

TEST(IdleServiceTest, IdleInhibitedStartsFalse) {
  auto backend = std::make_unique<FakeIdleBackend>();
  IdleService service(std::move(backend), false);
  EXPECT_FALSE(service.idleInhibited());
}

TEST(IdleServiceTest, SetIdleInhibitedDoesNotChangeStateWhenAcquireFails) {
  auto backend = std::make_unique<FakeIdleBackend>();
  auto inhibitor = std::make_unique<FakeIdleInhibitor>();
  inhibitor->acquire_result = false;
  auto* raw_inhibitor = inhibitor.get();
  IdleService service(std::move(backend), std::move(inhibitor), false);
  QSignalSpy spy(&service, &IdleService::idleInhibitedChanged);

  service.setIdleInhibited(true);

  EXPECT_FALSE(service.idleInhibited());
  EXPECT_FALSE(raw_inhibitor->held);
  EXPECT_EQ(raw_inhibitor->acquire_calls, 1);
  EXPECT_EQ(spy.count(), 0);
}

TEST(IdleServiceTest, SetIdleInhibitedEmitsOnlyAfterAcquireSucceeds) {
  auto backend = std::make_unique<FakeIdleBackend>();
  auto inhibitor = std::make_unique<FakeIdleInhibitor>();
  auto* raw_inhibitor = inhibitor.get();
  IdleService service(std::move(backend), std::move(inhibitor), false);
  QSignalSpy spy(&service, &IdleService::idleInhibitedChanged);

  service.setIdleInhibited(true);

  EXPECT_TRUE(service.idleInhibited());
  EXPECT_TRUE(raw_inhibitor->held);
  EXPECT_EQ(raw_inhibitor->acquire_calls, 1);
  EXPECT_EQ(spy.count(), 1);
}

TEST(IdleServiceTest, SessionLockedStartsFalse) {
  auto backend = std::make_unique<FakeIdleBackend>();
  IdleService service(std::move(backend), false);
  EXPECT_FALSE(service.sessionLocked());
}

// ─── IdleInhibitor ───────────────────────────────────────────────────────────
// Smoke tests: acquire() silently fails in the offscreen test env (no logind),
// but the invariants (not held initially, double-acquire safe) must hold.

TEST(IdleInhibitorTest, InitiallyNotHeld) {
  IdleInhibitor inhibitor;
  EXPECT_FALSE(inhibitor.isHeld());
}

TEST(IdleInhibitorTest, AcquireWithoutLogindDoesNotCrash) {
  IdleInhibitor inhibitor;
  [[maybe_unused]] const bool acquired = inhibitor.acquire(QStringLiteral("test reason"));
  inhibitor.release();
  EXPECT_FALSE(inhibitor.isHeld());
}

TEST(IdleInhibitorTest, DoubleReleaseIsSafe) {
  IdleInhibitor inhibitor;
  inhibitor.release();
  inhibitor.release();
  EXPECT_FALSE(inhibitor.isHeld());
}

// ─── ScreenSaverAdaptor ──────────────────────────────────────────────────────
// Tests cover cookie-table logic without registering on D-Bus.

class ScreenSaverAdaptorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    raw_backend_ = new FakeIdleBackend;
    idle_service_ = std::make_unique<IdleService>(std::unique_ptr<IdleBackend>(raw_backend_), false);
    auto inhibitor = std::make_unique<FakeIdleInhibitor>();
    raw_inhibitor_ = inhibitor.get();
    adaptor_ = std::make_unique<ScreenSaverAdaptor>(idle_service_.get(), std::move(inhibitor));
  }

  [[nodiscard]] FakeIdleBackend* backend() const { return raw_backend_; }
  [[nodiscard]] FakeIdleInhibitor* inhibitor() const { return raw_inhibitor_; }
  [[nodiscard]] ScreenSaverAdaptor* adaptor() const { return adaptor_.get(); }

 private:
  FakeIdleBackend* raw_backend_{nullptr};
  FakeIdleInhibitor* raw_inhibitor_{nullptr};
  std::unique_ptr<IdleService> idle_service_;
  std::unique_ptr<ScreenSaverAdaptor> adaptor_;
};

TEST_F(ScreenSaverAdaptorTest, InhibitReturnsDifferentCookies) {
  const uint cookie1 = adaptor()->Inhibit(QStringLiteral("app1"), QStringLiteral("reason1"));
  const uint cookie2 = adaptor()->Inhibit(QStringLiteral("app2"), QStringLiteral("reason2"));
  EXPECT_NE(cookie1, 0U);
  EXPECT_NE(cookie2, 0U);
  EXPECT_NE(cookie1, cookie2);
}

TEST_F(ScreenSaverAdaptorTest, InhibitFailureDoesNotIssueCookie) {
  inhibitor()->acquire_result = false;

  const uint cookie = adaptor()->Inhibit(QStringLiteral("app1"), QStringLiteral("reason1"));

  EXPECT_EQ(cookie, 0U);
  EXPECT_FALSE(inhibitor()->held);
}

TEST_F(ScreenSaverAdaptorTest, UnInhibitWithUnknownCookieDoesNotCrash) { adaptor()->UnInhibit(999999); }

TEST_F(ScreenSaverAdaptorTest, GetSessionIdleTimeDelegatesToService) {
  backend()->setIdleSecs(77);
  EXPECT_EQ(adaptor()->GetSessionIdleTime(), 77U);
}

TEST_F(ScreenSaverAdaptorTest, ActiveChangedSignalRelaysIdleChanged) {
  QSignalSpy spy(adaptor(), &ScreenSaverAdaptor::ActiveChanged);

  adaptor()->onIdleChanged(true);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).toBool());
}

TEST_F(ScreenSaverAdaptorTest, ActiveChangedRelaysResume) {
  QSignalSpy spy(adaptor(), &ScreenSaverAdaptor::ActiveChanged);

  adaptor()->onIdleChanged(false);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_FALSE(spy.at(0).at(0).toBool());
}

TEST_F(ScreenSaverAdaptorTest, MultipleInhibitsThenUnInhibitAll) {
  const uint first_cookie = adaptor()->Inhibit(QStringLiteral("app1"), QStringLiteral("r1"));
  const uint second_cookie = adaptor()->Inhibit(QStringLiteral("app2"), QStringLiteral("r2"));
  EXPECT_TRUE(inhibitor()->held);
  EXPECT_EQ(inhibitor()->acquire_calls, 1);

  adaptor()->UnInhibit(first_cookie);
  EXPECT_TRUE(inhibitor()->held);

  adaptor()->UnInhibit(second_cookie);
  EXPECT_FALSE(inhibitor()->held);
  EXPECT_EQ(inhibitor()->release_calls, 1);
}
