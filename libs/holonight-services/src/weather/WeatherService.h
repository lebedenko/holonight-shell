#pragma once

#include "IActivityGate.h"
#include "WeatherData.h"
#include "WeatherProvider.h"

#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>

#include <holonight_shell_config/config_structs.h>
#include <memory>

class ConfigService;

// QML singleton owning weather state. Drives the WeatherProvider on a timer, resolves location
// (config-first, IP-geolocation fallback), persists the last good response to disk, applies
// exponential backoff on failure, tracks staleness, and gates all activity on `configured`.
class WeatherService : public QObject, public IActivityGate {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  friend class WeatherServiceTest;

  Q_PROPERTY(bool showInBar READ showInBar NOTIFY preferencesChanged)
  Q_PROPERTY(bool compactMode READ compactMode NOTIFY preferencesChanged)
  Q_PROPERTY(bool showFeelsLike READ showFeelsLike NOTIFY preferencesChanged)
  Q_PROPERTY(bool showLocation READ showLocation NOTIFY preferencesChanged)
  Q_PROPERTY(QString temperatureUnit READ temperatureUnit NOTIFY preferencesChanged)
  Q_PROPERTY(QString windUnit READ windUnit NOTIFY preferencesChanged)
  Q_PROPERTY(QString pressureUnit READ pressureUnit NOTIFY preferencesChanged)
  Q_PROPERTY(QString attribution READ attribution NOTIFY preferencesChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
  Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
  Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)
  Q_PROPERTY(QString locationLabel READ locationLabel NOTIFY locationChanged)
  Q_PROPERTY(QVariant current READ currentVariant NOTIFY currentChanged)
  Q_PROPERTY(QVariantList hourly READ hourlyVariant NOTIFY forecastChanged)
  Q_PROPERTY(QVariantList daily READ dailyVariant NOTIFY forecastChanged)

 public:
  explicit WeatherService(ConfigService* config, QObject* parent = nullptr);
  // Test seam: inject a mock provider.
  explicit WeatherService(ConfigService* config, std::unique_ptr<WeatherProvider> provider, QObject* parent = nullptr);
  ~WeatherService() override;

  WeatherService(const WeatherService&) = delete;
  WeatherService& operator=(const WeatherService&) = delete;
  WeatherService(WeatherService&&) = delete;
  WeatherService& operator=(WeatherService&&) = delete;

  [[nodiscard]] bool configured() const { return configured_; }
  [[nodiscard]] bool hasData() const { return has_data_; }
  [[nodiscard]] bool stale() const { return stale_; }
  [[nodiscard]] QString locationLabel() const { return location_label_; }
  [[nodiscard]] QVariant currentVariant() const;
  [[nodiscard]] QVariantList hourlyVariant() const;
  [[nodiscard]] QVariantList dailyVariant() const;

  [[nodiscard]] bool showInBar() const { return preferences_.show_in_bar; }
  [[nodiscard]] bool compactMode() const { return preferences_.compact_mode; }
  [[nodiscard]] bool showFeelsLike() const { return preferences_.show_feels_like; }
  [[nodiscard]] bool showLocation() const { return preferences_.show_location; }
  [[nodiscard]] QString temperatureUnit() const;
  [[nodiscard]] QString windUnit() const;
  [[nodiscard]] QString pressureUnit() const;
  [[nodiscard]] QString attribution() const;
  [[nodiscard]] QString status() const { return status_; }
  Q_INVOKABLE [[nodiscard]] double displayTemperature(double celsius, const QString& unit = {}) const;
  Q_INVOKABLE [[nodiscard]] double displayWind(double kmh, const QString& unit = {}) const;
  Q_INVOKABLE [[nodiscard]] double displayPressure(double hpa, const QString& unit = {}) const;
  Q_INVOKABLE [[nodiscard]] QString formatTemperature(double celsius, const QString& unit = {}) const;

  // Called by ShellApplication::startServices(). Loads cache, resolves location, begins polling.
  void start();

  // Re-reads config and recomputes `configured`. Exposed for tests.
  void applyConfig();

  // Maps an OWM condition id + day/night flag to a bundled icon URL
  // (qrc:/HolonightShell/weather/wsymbol_*.svg). Unknown ids fall back to the unknown icon.
  Q_INVOKABLE [[nodiscard]] static QString iconPath(int condition_id, bool is_day);

 Q_SIGNALS:
  void preferencesChanged();
  void statusChanged();
  void configuredChanged();
  void hasDataChanged();
  void staleChanged();
  void locationChanged();
  void currentChanged();
  void forecastChanged();
  void authenticationFailed(const QString& message);

 public Q_SLOTS:
  void onIdleChanged(bool idle);

  // IActivityGate — pauses on lid close, resumes (with immediate fetch) on lid open.
  void pauseActivity() override;
  void resumeActivity() override;

 private:
  void onConfigChanged();
  void onRefreshTimer();
  void onStaleTimer();
  void onWeatherFetched(const CurrentWeather& weather, const QList<HourlyEntry>& hourly,
                        const QList<DailyEntry>& daily);
  void onFetchError(WeatherProvider::FetchErrorKind kind, const QString& message);
  void onGeoFetched(double lat, double lon, const WeatherLocation& location);
  void onGeoError(const QString& message);
  void onLocationFetched(const WeatherLocation& location);
  static void onLocationError(const QString& message);

  void connectProvider();
  void loadCache();
  void saveCache() const;
  void resolveLocation();
  void fetch();
  void maybeComplete();
  void evaluateStaleOnLoad();
  void startStaleTimer();
  void rebuildForecastVariants();

  void setConfigured(bool value);
  void setHasData(bool value);
  void setStale(bool value);
  void setResolvedLocation(const WeatherLocation& location);
  [[nodiscard]] WeatherLocation mergeConfiguredLocation(const WeatherLocation& location) const;
  [[nodiscard]] static QString formatLocationLabel(const WeatherLocation& location);
  void resetBackoff();
  void stepBackoff();
  [[nodiscard]] int nextBackoffMs() const;

  [[nodiscard]] bool computeConfigured() const;
  [[nodiscard]] static QString cachePath();

  ConfigService* config_{nullptr};
  std::unique_ptr<WeatherProvider> provider_;

  QTimer refresh_timer_;
  QTimer stale_timer_;

  // Backoff state. backoff_step_ == 0 means "no backoff" (use the base interval).
  int backoff_step_{0};
  static constexpr int kBackoffBaseSecs{1};
  static constexpr int kStaleThresholdSecs{3600};

  // Weather state.
  CurrentWeather current_;
  QList<HourlyEntry> hourly_;
  QList<DailyEntry> daily_;
  QVariantList hourly_variant_;
  QVariantList daily_variant_;
  qint64 fetched_at_{0};
  bool configured_{false};
  bool has_data_{false};
  bool stale_{true};

  // Location state.
  double lat_{0.0};
  double lon_{0.0};
  bool location_resolved_{false};
  WeatherLocation location_;
  QString location_label_;

  // Config snapshot, refreshed in applyConfig().
  HoloNight::ShellConfig::WeatherConfig preferences_;
  QString status_;
  [[nodiscard]] QString cacheIdentity() const;
  void setStatus(const QString& status);
  QString api_key_;
  QString geo_api_key_;
  QString configured_city_;
  QString configured_country_;
  QString lang_{"en"};
  int refresh_interval_{600};

  // In-flight tracking for the fetch cycle.
  bool started_{false};
  bool geo_pending_{false};
  bool fetch_pending_{false};
  bool fetch_failed_{false};
  bool idle_paused_{false};
  bool lid_paused_{false};
};
