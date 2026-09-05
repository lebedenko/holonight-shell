#pragma once

#include "AuthenticationPromptModel.h"

#include <QtQml/qqmlregistration.h>

struct AuthenticationPromptModelForeign {
  Q_GADGET
  QML_FOREIGN(Holonight::Authentication::AuthenticationPromptModel)
  QML_NAMED_ELEMENT(AuthenticationPromptModel)
  QML_UNCREATABLE("AuthenticationPromptModel is owned by the authentication frontend")
};
