#include "HttpSyncClient.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <utility>

namespace {

SyncError makeError(const QString& account_label, const QString& detail,
                    SyncError::Kind kind = SyncError::Kind::ConnectError) {
  return {.kind = kind, .message = QStringLiteral("%1: %2").arg(account_label, detail)};
}

bool isNetworkLayerError(QNetworkReply::NetworkError error) {
  return error >= QNetworkReply::ConnectionRefusedError && error <= QNetworkReply::UnknownNetworkError;
}

}  // namespace

HttpSyncClient::HttpSyncClient(QString account_label, int timeout_ms)
    : account_label_(std::move(account_label)), timeout_ms_(timeout_ms) {}

std::expected<QByteArray, SyncError> HttpSyncClient::get(const QNetworkRequest& request) const {
  return runAndAwait(request, "GET", {});
}

std::expected<QByteArray, SyncError> HttpSyncClient::sendCustomRequest(const QNetworkRequest& request,
                                                                       const QByteArray& verb,
                                                                       const QByteArray& body) const {
  return runAndAwait(request, verb, body);
}

std::expected<QByteArray, SyncError> HttpSyncClient::runAndAwait(const QNetworkRequest& request, const QByteArray& verb,
                                                                 const QByteArray& body) const {
  QNetworkAccessManager nam;
  QNetworkReply* reply = nam.sendCustomRequest(request, verb, body);

  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QTimer::singleShot(timeout_ms_, &loop, &QEventLoop::quit);
  loop.exec();

  if (reply->isRunning()) {
    reply->abort();
    reply->deleteLater();
    return std::unexpected(
        makeError(account_label_, QStringLiteral("connection timed out"), SyncError::Kind::NetworkError));
  }

  if (reply->error() != QNetworkReply::NoError) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString detail = status > 0 ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    const SyncError::Kind kind =
        isNetworkLayerError(reply->error()) ? SyncError::Kind::NetworkError : SyncError::Kind::ConnectError;
    reply->deleteLater();
    return std::unexpected(makeError(account_label_, detail, kind));
  }

  QByteArray response_body = reply->readAll();
  reply->deleteLater();
  return response_body;
}
