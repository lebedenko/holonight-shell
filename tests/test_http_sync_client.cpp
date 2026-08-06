#include "calendar/CalendarTypes.h"
#include "calendar/HttpSyncClient.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include <gtest/gtest.h>

namespace {

QNetworkRequest requestFor(const QTcpServer& server) {
  return QNetworkRequest{QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort()))};
}

void keepAcceptedSocketOpen(QTcpServer& server) {
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
    QTcpSocket* socket = server.nextPendingConnection();
    if (socket != nullptr) {
      socket->setParent(&server);
    }
  });
}

}  // namespace

TEST(HttpSyncClientTest, TimesOutOnHungServer) {
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  // Accept the connection but never write a response; keep the socket alive under the server
  // so it isn't closed (and thus doesn't count as a "remote closed" error) before the timeout fires.
  keepAcceptedSocketOpen(server);

  HttpSyncClient client(QStringLiteral("test-account"), 1000);
  const QNetworkRequest request = requestFor(server);

  QElapsedTimer timer;
  timer.start();
  const auto result = client.get(request);
  const qint64 elapsed_ms = timer.elapsed();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SyncError::Kind::NetworkError);
  EXPECT_TRUE(result.error().message.contains(QStringLiteral("timed out")));
  EXPECT_GE(elapsed_ms, 900);
  EXPECT_LT(elapsed_ms, 3000);
}

TEST(HttpSyncClientTest, ReturnsErrorOnImmediateClose) {
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
    QTcpSocket* socket = server.nextPendingConnection();
    if (socket != nullptr) {
      socket->disconnectFromHost();
      socket->deleteLater();
    }
  });

  HttpSyncClient client(QStringLiteral("test-account"), 5000);
  const QNetworkRequest request = requestFor(server);

  const auto result = client.get(request);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SyncError::Kind::NetworkError);
  EXPECT_FALSE(result.error().message.contains(QStringLiteral("timed out")));
}

TEST(HttpSyncClientTest, ReturnsConnectErrorOnHttpStatusFailure) {
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
    QTcpSocket* socket = server.nextPendingConnection();
    if (socket == nullptr) {
      return;
    }
    QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
      socket->readAll();
      const QByteArray response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      socket->write(response);
      socket->flush();
      socket->disconnectFromHost();
    });
  });

  HttpSyncClient client(QStringLiteral("test-account"), 5000);
  const QNetworkRequest request = requestFor(server);

  const auto result = client.get(request);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SyncError::Kind::ConnectError);
  EXPECT_TRUE(result.error().message.contains(QStringLiteral("404")));
}

TEST(HttpSyncClientTest, ReturnsBodyOnValidResponse) {
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  const QByteArray body = "hello from fake server";
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, &body]() {
    QTcpSocket* socket = server.nextPendingConnection();
    if (socket == nullptr) {
      return;
    }
    QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, &body]() {
      socket->readAll();
      const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                                  "\r\nConnection: close\r\n\r\n" + body;
      socket->write(response);
      socket->flush();
      socket->disconnectFromHost();
    });
  });

  HttpSyncClient client(QStringLiteral("test-account"), 5000);
  const QNetworkRequest request = requestFor(server);

  const auto result = client.get(request);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, body);
}
