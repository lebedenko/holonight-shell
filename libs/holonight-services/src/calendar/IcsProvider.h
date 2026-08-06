#pragma once

#include "ConfigService.h"
#include "HttpSyncClient.h"
#include "ICalendarProvider.h"

class IcsProvider final : public ICalendarProvider {
 public:
  explicit IcsProvider(CalendarIcsAccountConfig config);
  ~IcsProvider() override = default;

  IcsProvider(const IcsProvider&) = delete;
  IcsProvider& operator=(const IcsProvider&) = delete;
  IcsProvider(IcsProvider&&) = delete;
  IcsProvider& operator=(IcsProvider&&) = delete;

  [[nodiscard]] QString accountName() const override { return config_.account_name; }
  [[nodiscard]] QString providerType() const override { return QStringLiteral("ics"); }

  [[nodiscard]] std::expected<void, SyncError> testConnection() override;
  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& range) override;

 private:
  // Performs HTTP GET to config_.url with 10s timeout.
  // Returns response body on success, SyncError on HTTP/network failure.
  [[nodiscard]] std::expected<QByteArray, SyncError> httpGet() const;

  // Deduplicates events by UID within the same feed, keeping the one with the latest DTSTAMP.
  [[nodiscard]] static QList<CalendarEvent> deduplicateByUid(QList<CalendarEvent> events);

  friend class IcsProviderTest;

  CalendarIcsAccountConfig config_;
  HttpSyncClient http_client_;
};
