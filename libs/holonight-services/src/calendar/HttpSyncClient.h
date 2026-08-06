#pragma once

#include "CalendarTypes.h"

#include <QByteArray>
#include <QString>

#include <expected>

class QNetworkRequest;

// Synchronous (blocking, worker-thread-only) HTTP helper shared by CalDavProvider and
// IcsProvider. Runs a QNetworkAccessManager request through a local QEventLoop with an active
// timeout: unlike a naive QTimer::singleShot-races-finished pattern, a timeout aborts the reply
// before returning, so a hung/non-responding server always yields SyncError rather than a
// reply that looks "successful" (NoError, empty body) because it was still in flight when read.
class HttpSyncClient {
 public:
  explicit HttpSyncClient(QString account_label, int timeout_ms = kDefaultTimeoutMs);
  ~HttpSyncClient() = default;

  HttpSyncClient(const HttpSyncClient&) = delete;
  HttpSyncClient& operator=(const HttpSyncClient&) = delete;
  HttpSyncClient(HttpSyncClient&&) = delete;
  HttpSyncClient& operator=(HttpSyncClient&&) = delete;

  static constexpr int kDefaultTimeoutMs{10000};

  // Plain GET.
  [[nodiscard]] std::expected<QByteArray, SyncError> get(const QNetworkRequest& request) const;

  // Custom-verb request (PROPFIND/REPORT/etc.) with an optional body, for WebDAV/CalDAV.
  [[nodiscard]] std::expected<QByteArray, SyncError> sendCustomRequest(const QNetworkRequest& request,
                                                                       const QByteArray& verb,
                                                                       const QByteArray& body = {}) const;

 private:
  [[nodiscard]] std::expected<QByteArray, SyncError> runAndAwait(const QNetworkRequest& request, const QByteArray& verb,
                                                                 const QByteArray& body) const;

  QString account_label_;
  int timeout_ms_;
};
