#pragma once

#include "ConfigService.h"
#include "HttpSyncClient.h"
#include "ICalendarProvider.h"
#include "LibsecretCredentialStorage.h"

#include <QStringList>

class CalDavProvider final : public ICalendarProvider {
 public:
  explicit CalDavProvider(CalendarCaldavAccountConfig config, const LibsecretCredentialStorage* credentials);
  ~CalDavProvider() override = default;

  CalDavProvider(const CalDavProvider&) = delete;
  CalDavProvider& operator=(const CalDavProvider&) = delete;
  CalDavProvider(CalDavProvider&&) = delete;
  CalDavProvider& operator=(CalDavProvider&&) = delete;

  [[nodiscard]] QString accountName() const override { return config_.account_name; }
  [[nodiscard]] QString providerType() const override { return QStringLiteral("caldav"); }

  [[nodiscard]] std::expected<void, SyncError> testConnection() override;
  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& range) override;

 private:
  // Resolves the CalDAV principal URL: tries .well-known/caldav first, falls back to config url.
  [[nodiscard]] QString resolvePrincipalUrl(const QString& password) const;

  // Discovers all calendar collection URLs from the principal via PROPFIND Depth:1.
  // Returns a filtered list (include/exclude applied).
  [[nodiscard]] std::expected<QStringList, SyncError> discoverCalendars(const QString& principal_url,
                                                                        const QString& password) const;

  // Fetches events from one calendar via CALDAV:calendar-query REPORT.
  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchCalendarEvents(const QString& calendar_url,
                                                                                   const QString& password,
                                                                                   const DateRange& range) const;

  // Builds the Authorization: Basic header value for this account.
  [[nodiscard]] static QString basicAuthHeader(const QString& username, const QString& password);

  CalendarCaldavAccountConfig config_;
  const LibsecretCredentialStorage* credentials_;
  HttpSyncClient http_client_;
};
