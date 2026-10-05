#include "CompositorBackend.h"
#include "CompositorService.h"
#include "WindowActivation.h"

#include <gtest/gtest.h>
#include <memory>

namespace {
class RejectingActivationBackend final : public CompositorBackend {
 public:
  void start() override {}
  void activateWorkspace(const QString& /*workspace_id*/) override {}
};

WindowActivationRequest request(QList<quint32> lineage, QString title = {}) {
  return {.process_lineage = std::move(lineage), .title_hint = std::move(title)};
}
}  // namespace

TEST(CompositorServiceWindowActivation, GatesRequestsAndBackendsRejectByDefault) {
  auto backend = std::make_unique<RejectingActivationBackend>();
  CompositorService service(std::move(backend));

  EXPECT_EQ(service.requestWindowActivation(request({42})), WindowActivationResult::Disconnected);
  service.publishSnapshotForTest({.connected = true});
  EXPECT_FALSE(service.canActivateWindows());
  EXPECT_EQ(service.requestWindowActivation(request({42})), WindowActivationResult::Unsupported);

  service.publishSnapshotForTest({.connected = true, .capabilities = {.window_activation = true}});
  EXPECT_TRUE(service.canActivateWindows());
  EXPECT_EQ(service.requestWindowActivation(request({0})), WindowActivationResult::InvalidRequest);
  EXPECT_EQ(service.requestWindowActivation(request({42})), WindowActivationResult::Unsupported);
}
