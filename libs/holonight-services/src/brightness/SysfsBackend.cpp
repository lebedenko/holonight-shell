#include "SysfsBackend.h"

#include "LogindSessionResolver.h"
#include "SysfsValueReader.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSocketNotifier>

#include <array>
#include <sys/inotify.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcBrightness, "holonight.brightness")

namespace {
constexpr auto kLogindService = "org.freedesktop.login1";
constexpr auto kLogindSessionIface = "org.freedesktop.login1.Session";
constexpr auto kBacklightDir = "/sys/class/backlight";
}  // namespace

SysfsBackend::SysfsBackend(QObject* parent) : BrightnessBackend(parent) {
  selectDevice();
  if (device_name_.isEmpty()) {
    return;
  }
  resolveSessionPath();
  setupInotify();
}

SysfsBackend::~SysfsBackend() {
  if (iwd_ >= 0) {
    inotify_rm_watch(ifd_, iwd_);
  }
  if (ifd_ >= 0) {
    close(ifd_);
  }
}

int SysfsBackend::currentBrightness() const { return readBrightness(); }

void SysfsBackend::selectDevice() {
  const QDir dir{QLatin1String(kBacklightDir)};
  const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

  for (const QString& entry : entries) {
    static const QRegularExpression kValidName(QStringLiteral("^[a-zA-Z0-9_-]+$"));
    if (!kValidName.match(entry).hasMatch()) {
      qCWarning(lcBrightness) << "Skipping backlight device with unsafe name:" << entry;
      continue;
    }

    const auto max_value = readSysfsInteger(QStringLiteral("%1/%2/max_brightness").arg(dir.absolutePath(), entry));
    if (!max_value.has_value()) {
      continue;
    }

    if (*max_value > max_brightness_) {
      max_brightness_ = *max_value;
      device_name_ = entry;
    }
  }

  if (!device_name_.isEmpty()) {
    brightness_path_ = QStringLiteral("%1/%2/brightness").arg(dir.absolutePath(), device_name_);
    qCInfo(lcBrightness) << "Selected backlight device:" << device_name_ << "max_brightness:" << max_brightness_;
  }
}

void SysfsBackend::resolveSessionPath() {
  session_path_ = resolveActiveLogindSessionPath();
  if (session_path_.isEmpty()) {
    qCWarning(lcBrightness) << "Could not resolve logind session path; brightness writes disabled";
  }
}

void SysfsBackend::setupInotify() {
  ifd_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
  if (ifd_ < 0) {
    qCWarning(lcBrightness) << "inotify_init1 failed; external brightness changes will not update slider";
    return;
  }
  iwd_ = inotify_add_watch(ifd_, brightness_path_.toLocal8Bit().constData(), IN_MODIFY);
  if (iwd_ < 0) {
    qCWarning(lcBrightness) << "inotify_add_watch failed for" << brightness_path_;
    return;
  }
  notifier_ = std::make_unique<QSocketNotifier>(ifd_, QSocketNotifier::Read, this);
  connect(notifier_.get(), &QSocketNotifier::activated, this, [this]() { onInotifyEvent(); });
  qCInfo(lcBrightness) << "inotify watch set up on" << brightness_path_;
}

int SysfsBackend::readBrightness() const { return readSysfsInteger(brightness_path_).value_or(0); }

void SysfsBackend::setBrightness(int value) {
  if (session_path_.isEmpty()) {
    qCWarning(lcBrightness) << "setBrightness: logind session path unavailable";
    return;
  }
  QDBusInterface session(QLatin1String(kLogindService), session_path_, QLatin1String(kLogindSessionIface),
                         QDBusConnection::systemBus());
  auto* watcher =
      new QDBusPendingCallWatcher(session.asyncCall(QStringLiteral("SetBrightness"), QStringLiteral("backlight"),
                                                    device_name_, static_cast<quint32>(value)),
                                  this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    if (ptr->isError()) {
      qCWarning(lcBrightness) << "SetBrightness D-Bus call failed:" << ptr->error().message();
    }
  });
}

void SysfsBackend::onInotifyEvent() {
  // Drain all pending inotify events before doing one sysfs read, coalescing rapid
  // Fn-key bursts into a single brightnessChanged emission.
  std::array<char, sizeof(inotify_event) + NAME_MAX + 1> buf{};
  while (read(ifd_, buf.data(), buf.size()) > 0) {
  }

  const int new_val = readBrightness();
  emit brightnessChanged(new_val);
}
