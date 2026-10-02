#pragma once
#include "CompositorService.h"
#include "NumberedWorkspaceProvider.h"

class WorkspacePresentation final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(bool sectionVisible READ sectionVisible NOTIFY revisionChanged)
  Q_PROPERTY(bool useNumericWorkspacePresentation READ useNumericWorkspacePresentation NOTIFY revisionChanged)
  Q_PROPERTY(int workspaceDisplayCount READ workspaceDisplayCount WRITE setWorkspaceDisplayCount NOTIFY
                 workspaceDisplayCountChanged)
 public:
  WorkspacePresentation(CompositorService* service, NumberedWorkspaceProvider* provider, QObject* parent = nullptr);
  [[nodiscard]] int revision() const { return service_->revision(); }
  [[nodiscard]] bool useNumericWorkspacePresentation() const { return eligible_; }
  [[nodiscard]] bool sectionVisible() const {
    return service_->connected() && service_->canListWorkspaces() &&
           (eligible_ || service_->workspaces()->rowCount() >= 2);
  }
  [[nodiscard]] int workspaceDisplayCount() const { return display_count_; }
  void setWorkspaceDisplayCount(int count);
  Q_INVOKABLE int firstVisibleWorkspaceRow() const;
  Q_INVOKABLE int viewportStart(const QString& output, int pan) const;
  Q_INVOKABLE QVariantList numberedSlots(int start, int count) const;
  Q_INVOKABLE void activateNumberedSlot(int slot);
  Q_INVOKABLE [[nodiscard]] int activeNumericWorkspaceForOutput(const QString& output) const;
  Q_INVOKABLE [[nodiscard]] QString numericWorkspaceVisualState(int slot) const;
  Q_INVOKABLE [[nodiscard]] bool hasNavigableNumericWorkspaceAtOrBeyond(int slot) const;
  Q_INVOKABLE [[nodiscard]] bool hasUrgentNumericWorkspaceAtOrBeyond(int slot) const;
  Q_INVOKABLE [[nodiscard]] int firstUrgentNumericWorkspaceAtOrBeyond(int slot) const;
  Q_INVOKABLE [[nodiscard]] bool hasUrgentNumericWorkspaceBefore(int slot) const;
  Q_INVOKABLE [[nodiscard]] int lastUrgentNumericWorkspaceBefore(int slot) const;
 Q_SIGNALS:
  void revisionChanged();
  void workspaceDisplayCountChanged();

 private:
  CompositorService* service_;
  NumberedWorkspaceProvider* provider_;
  QHash<QString, int> slots_;
  bool eligible_{false};
  int display_count_{5};
};
