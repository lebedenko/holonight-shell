#pragma once

#include "AuthenticationPromptModel.h"

#include <QObject>

#include <functional>
#include <memory>

namespace Holonight::Authentication {

// Presentation-only enrichment; identity eligibility remains owned by Polkit.
class AccountProfileResolver final : public QObject {
 public:
  using ProfileCallback = std::function<void(const QVariantMap&)>;
  using ProfileLookup = std::function<void(uint, QObject*, ProfileCallback)>;
  explicit AccountProfileResolver(AuthenticationPromptModel* model, ProfileLookup lookup = {},
                                  QObject* parent = nullptr);
  void resolveCurrentRequest();
  static Identity localProfile(Identity identity);
  static Identity enrichedProfile(Identity identity, const QVariantMap& properties);
  static QUrl localAvatar(const QString& path);

 private:
  AuthenticationPromptModel* model_;
  ProfileLookup lookup_;
  std::unique_ptr<QObject> request_scope_;
};
}  // namespace Holonight::Authentication
