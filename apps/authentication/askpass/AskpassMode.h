#pragma once
#include "AuthenticationPromptModel.h"

#include <QString>
namespace Holonight::Authentication {
AuthenticationPromptModel::FrontendKind askpassFrontendKind(const QString& basename);
AuthenticationPromptModel::InputMode askpassMode(const QString& basename, const QString& prompt_hint);
}  // namespace Holonight::Authentication
