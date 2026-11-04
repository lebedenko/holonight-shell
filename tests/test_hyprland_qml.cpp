#include "IntegrationLoader.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QVariantList>

#include <gtest/gtest.h>

class SpecialModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList specialWorkspaces READ specialWorkspaces NOTIFY changed)
 public:
  [[nodiscard]] QVariantList specialWorkspaces() const { return rows; }
  Q_INVOKABLE void activateSpecialWorkspace(const QString& identifier) { activated = identifier; }
  QVariantList rows{
      {
          QVariantMap{
              {"id", "special:magic"},
              {"name", "magic"},
              {"active", true},
              {"urgent", false},
              {"occupied", true},
              {"monitorNames", QStringList{"DP-1"}},
          },
      },
  };
  QString activated;
 Q_SIGNALS:
  void changed();
};
TEST(HyprlandQml, PrivateContributionUsesInjectedInstanceAndMonitor) {
  QProcessEnvironment environment;
  environment.insert("XDG_CURRENT_DESKTOP", "Hyprland");
  IntegrationLoader loader({PLUGIN_DIRECTORY}, environment);
  ASSERT_EQ(loader.backendName(), "hyprland");
  QQmlEngine engine;
  engine.addImportPath(QStringLiteral("qrc:/"));
  QQmlComponent component(&engine, loader.componentUrl());
  QTRY_VERIFY_WITH_TIMEOUT(!component.isLoading(), 2000);
  ASSERT_FALSE(component.isError()) << component.errorString().toStdString();
  QQuickWindow window;
  SpecialModel model;
  std::unique_ptr<QObject> item(component.createWithInitialProperties(
      {{"barMonitorName", "DP-1"}, {"contributionModel", QVariant::fromValue(&model)}}));
  ASSERT_NE(item, nullptr) << component.errorString().toStdString();
  qobject_cast<QQuickItem*>(item.get())->setParentItem(window.contentItem());
  window.show();
  EXPECT_EQ(item->property("implicitWidth").toDouble(), 32);
  EXPECT_EQ(item->property("implicitHeight").toDouble(), 32);
  EXPECT_TRUE(item->property("visible").toBool());
  const auto findDot = [](auto&& self, QQuickItem* parent) -> QQuickItem* {
    if (parent->objectName() == "specialWorkspaceDot") {
      return parent;
    }
    for (auto* child : parent->childItems()) {
      if (auto* found = self(self, child)) {
        return found;
      }
    }
    return nullptr;
  };
  auto* dot = findDot(findDot, qobject_cast<QQuickItem*>(item.get()));
  ASSERT_NE(dot, nullptr);
  EXPECT_TRUE(dot->property("activeOnCurrentMonitor").toBool());
  ASSERT_TRUE(QMetaObject::invokeMethod(dot, "activated"));
  EXPECT_EQ(model.activated, "special:magic");
  item->setProperty("barMonitorName", "DP-2");
  EXPECT_TRUE(dot->property("activeOnAnotherMonitor").toBool());
  const auto populatedRows = model.rows;
  auto second = model.rows.first().toMap();
  second.insert("id", "special:keymapp");
  second.insert("name", "keymapp");
  model.rows.append(second);
  emit model.changed();
  QTRY_COMPARE(item->property("implicitWidth").toDouble(), 72);
  EXPECT_EQ(item->property("implicitHeight").toDouble(), 32);
  QList<QQuickItem*> dots;
  const auto collectDots = [&dots](auto&& self, QQuickItem* parent) -> void {
    if (parent->objectName() == "specialWorkspaceDot") dots.append(parent);
    for (auto* child : parent->childItems()) self(self, child);
  };
  collectDots(collectDots, qobject_cast<QQuickItem*>(item.get()));
  ASSERT_EQ(dots.size(), 2);
  ASSERT_TRUE(QMetaObject::invokeMethod(dots[1], "activated"));
  EXPECT_EQ(model.activated, "special:keymapp");
  ASSERT_TRUE(QMetaObject::invokeMethod(dots[0], "activated"));
  EXPECT_EQ(model.activated, "special:magic");
  model.rows.clear();
  emit model.changed();
  EXPECT_EQ(item->property("implicitWidth").toDouble(), 0);
  EXPECT_EQ(item->property("implicitHeight").toDouble(), 0);
  EXPECT_FALSE(item->property("visible").toBool());
  model.rows = populatedRows;
  emit model.changed();
  QTRY_COMPARE(item->property("implicitWidth").toDouble(), 32);
  EXPECT_TRUE(item->property("visible").toBool());
}
#include "test_hyprland_qml.moc"
