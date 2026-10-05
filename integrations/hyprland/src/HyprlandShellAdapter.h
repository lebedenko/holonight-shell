#pragma once
#include "CompositorBackend.h"
#include "NumberedWorkspaceProvider.h"

#include <holonight_system/compositor/CompositorFactory.h>
#include <holonight_system/compositor/SpecialWorkspaceProvider.h>

class HyprlandShellAdapter final : public CompositorBackend, public NumberedWorkspaceProvider {
  Q_OBJECT
  Q_PROPERTY(QVariantList specialWorkspaces READ specialWorkspaces NOTIFY specialWorkspacesChanged)
 public:
  HyprlandShellAdapter() : backend_(createCompositorBackend(QStringLiteral("hyprland"))) {
    connect(backend_.get(), &CompositorBackend::snapshotReady, this, [this](CompositorSnapshot snapshot) {
      emit specialWorkspacesChanged();
      emit snapshotReady(std::move(snapshot));
    });
  }
  void start() override { backend_->start(); }
  void requestSnapshotRefresh() override { backend_->requestSnapshotRefresh(); }
  void activateWorkspace(const QString& identifier) override { backend_->activateWorkspace(identifier); }
  WindowActivationResult requestWindowActivation(const WindowActivationRequest& request) override {
    return backend_->requestWindowActivation(request);
  }
  [[nodiscard]] NumberedWorkspaceState numberedWorkspaces() const override {
    auto* numbered = dynamic_cast<NumberedWorkspaceProvider*>(backend_.get());
    return (numbered != nullptr) ? numbered->numberedWorkspaces() : NumberedWorkspaceState{};
  }
  void activateNumberedSlot(int slot) override {
    if (auto* numbered = dynamic_cast<NumberedWorkspaceProvider*>(backend_.get())) {
      numbered->activateNumberedSlot(slot);
    }
  }
  [[nodiscard]] QVariantList specialWorkspaces() const {
    auto* special = dynamic_cast<SpecialWorkspaceProvider*>(backend_.get());
    return (special != nullptr) ? special->specialWorkspaces() : QVariantList{};
  }
  Q_INVOKABLE void activateSpecialWorkspace(const QString& identifier) {
    if (auto* special = dynamic_cast<SpecialWorkspaceProvider*>(backend_.get())) {
      special->activateSpecialWorkspace(identifier);
    }
  }
 signals:
  void specialWorkspacesChanged();

 private:
  std::unique_ptr<CompositorBackend> backend_;
};
