#pragma once

#include "PolkitRequestCoordinator.h"

#include <memory>

namespace Holonight::Authentication {

std::unique_ptr<PamSession> createPolkitSession(const QString& identity, const QString& cookie, quint64 generation,
                                                PamSession::Callbacks callbacks);

}  // namespace Holonight::Authentication
