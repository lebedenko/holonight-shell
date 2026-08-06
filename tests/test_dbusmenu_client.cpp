#include "DbusMenuClient.h"
#include "DbusMenuItem.h"

#include <QVariantMap>

#include <gtest/gtest.h>

// DbusMenuClient::parseItem() reads from a live QDBusArgument produced by the D-Bus framework
// (read mode). Constructing a read-mode arg without a real D-Bus connection is not possible via
// public Qt API. These tests therefore exercise the property-extraction logic through
// DbusMenuClient::makeItem(), which is the internal delegate that parseItem() calls after
// extracting id/props/children from the wire. Integration tests cover the D-Bus extraction path.

TEST(DbusMenuClient, ParsesStandardItem) {
  QVariantMap props;
  props.insert(QStringLiteral("label"), QStringLiteral("Open"));
  props.insert(QStringLiteral("enabled"), true);
  props.insert(QStringLiteral("visible"), true);

  const DbusMenuItem item = DbusMenuClient::makeItem(1, props);
  EXPECT_EQ(item.id, 1);
  EXPECT_EQ(item.label, QStringLiteral("Open"));
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.visible);
  EXPECT_EQ(item.type, QStringLiteral("standard"));
  EXPECT_TRUE(item.children.isEmpty());
}

TEST(DbusMenuClient, ParsesSeparator) {
  QVariantMap props;
  props.insert(QStringLiteral("type"), QStringLiteral("separator"));

  const DbusMenuItem item = DbusMenuClient::makeItem(2, props);
  EXPECT_EQ(item.id, 2);
  EXPECT_EQ(item.type, QStringLiteral("separator"));
}

TEST(DbusMenuClient, ParsesDisabledItem) {
  QVariantMap props;
  props.insert(QStringLiteral("label"), QStringLiteral("Grayed"));
  props.insert(QStringLiteral("enabled"), false);
  props.insert(QStringLiteral("visible"), true);

  const DbusMenuItem item = DbusMenuClient::makeItem(3, props);
  EXPECT_FALSE(item.enabled);
  EXPECT_TRUE(item.visible);
}

TEST(DbusMenuClient, ParsesCheckmarkItem) {
  QVariantMap props;
  props.insert(QStringLiteral("label"), QStringLiteral("Mute"));
  props.insert(QStringLiteral("toggle-type"), QStringLiteral("checkmark"));
  props.insert(QStringLiteral("toggle-state"), 1);

  const DbusMenuItem item = DbusMenuClient::makeItem(4, props);
  EXPECT_EQ(item.toggle_type, QStringLiteral("checkmark"));
  EXPECT_EQ(item.toggle_state, 1);
}

TEST(DbusMenuClient, ParsesOneChildLevel) {
  QVariantMap child_props;
  child_props.insert(QStringLiteral("label"), QStringLiteral("Child"));
  const DbusMenuItem child = DbusMenuClient::makeItem(10, child_props);

  QVariantMap parent_props;
  parent_props.insert(QStringLiteral("label"), QStringLiteral("Parent"));
  const DbusMenuItem parent = DbusMenuClient::makeItem(1, parent_props, {child});

  ASSERT_EQ(parent.children.size(), 1);
  EXPECT_EQ(parent.children.at(0).id, 10);
  EXPECT_EQ(parent.children.at(0).label, QStringLiteral("Child"));
}

TEST(DbusMenuClient, NestedChildrenPreservedThroughMakeItem) {
  // Verify makeItem correctly assembles an arbitrarily deep tree (kMaxDepth levels).
  // The depth limit only applies inside parseItem (D-Bus path); makeItem has no limit.
  DbusMenuItem leaf;
  leaf.id = 99;
  leaf.label = QStringLiteral("Leaf");

  DbusMenuItem current = leaf;
  for (int depth = DbusMenuClient::kMaxDepth - 1; depth >= 0; --depth) {
    QVariantMap wrapper_props;
    wrapper_props.insert(QStringLiteral("label"), QStringLiteral("D%1").arg(depth));
    current = DbusMenuClient::makeItem(depth, wrapper_props, {current});
  }

  const DbusMenuItem* node = &current;
  for (int depth = 0; depth < DbusMenuClient::kMaxDepth; ++depth) {
    ASSERT_EQ(node->children.size(), 1) << "Expected child at depth " << depth;
    node = node->children.data();
  }
  EXPECT_TRUE(node->children.isEmpty());
}

TEST(DbusMenuClient, MaxDepthConstantIs5) {
  static_assert(DbusMenuClient::kMaxDepth == 5, "update depth-limit tests if this changes");
  EXPECT_EQ(DbusMenuClient::kMaxDepth, 5);
}

TEST(DbusMenuClient, DefaultsForMissingProperties) {
  QVariantMap props;
  props.insert(QStringLiteral("label"), QStringLiteral("Minimal"));

  const DbusMenuItem item = DbusMenuClient::makeItem(5, props);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.visible);
  EXPECT_EQ(item.type, QStringLiteral("standard"));
  EXPECT_EQ(item.toggle_state, -1);
}

TEST(DbusMenuClient, StripsMnemonicUnderscore) {
  // DbusMenuModel strips leading _ in data(LabelRole); verify raw label is preserved in struct.
  QVariantMap props;
  props.insert(QStringLiteral("label"), QStringLiteral("_File"));

  const DbusMenuItem item = DbusMenuClient::makeItem(6, props);
  EXPECT_EQ(item.label, QStringLiteral("_File"));
}

TEST(DbusMenuModel, ExposesAllRoles) {
  QList<DbusMenuItem> items = {
      DbusMenuItem{.id = 1,
                   .label = QStringLiteral("Cut"),
                   .type = QStringLiteral("standard"),
                   .icon_name = QStringLiteral("edit-cut"),
                   .enabled = true,
                   .visible = true,
                   .toggle_type = QString{},
                   .toggle_state = -1},
      DbusMenuItem{.id = 2,
                   .label = QString{},
                   .type = QStringLiteral("separator"),
                   .enabled = true,
                   .visible = true,
                   .toggle_state = -1},
  };

  DbusMenuModel model(items);
  ASSERT_EQ(model.rowCount(), 2);

  const QModelIndex idx0 = model.index(0, 0);
  EXPECT_EQ(model.data(idx0, DbusMenuModel::IdRole).toInt(), 1);
  EXPECT_EQ(model.data(idx0, DbusMenuModel::LabelRole).toString(), QStringLiteral("Cut"));
  EXPECT_EQ(model.data(idx0, DbusMenuModel::TypeRole).toString(), QStringLiteral("standard"));
  EXPECT_EQ(model.data(idx0, DbusMenuModel::IconNameRole).toString(), QStringLiteral("edit-cut"));
  EXPECT_TRUE(model.data(idx0, DbusMenuModel::EnabledRole).toBool());
  EXPECT_FALSE(model.data(idx0, DbusMenuModel::HasSubmenuRole).toBool());

  const QModelIndex idx1 = model.index(1, 0);
  EXPECT_EQ(model.data(idx1, DbusMenuModel::TypeRole).toString(), QStringLiteral("separator"));
}

TEST(DbusMenuModel, RoleNamesExposeQmlContract) {
  DbusMenuModel model({});
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(DbusMenuModel::IdRole), "itemId");
  EXPECT_EQ(roles.value(DbusMenuModel::LabelRole), "label");
  EXPECT_EQ(roles.value(DbusMenuModel::TypeRole), "type");
  EXPECT_EQ(roles.value(DbusMenuModel::IconNameRole), "iconName");
  EXPECT_EQ(roles.value(DbusMenuModel::EnabledRole), "itemEnabled");
  EXPECT_EQ(roles.value(DbusMenuModel::VisibleRole), "itemVisible");
  EXPECT_EQ(roles.value(DbusMenuModel::ToggleTypeRole), "toggleType");
  EXPECT_EQ(roles.value(DbusMenuModel::ToggleStateRole), "toggleState");
  EXPECT_EQ(roles.value(DbusMenuModel::HasSubmenuRole), "hasSubmenu");
  EXPECT_EQ(roles.size(), 9);
}

TEST(DbusMenuModel, SubmenuAtReturnsChildModel) {
  DbusMenuItem child{.id = 10,
                     .label = QStringLiteral("Child"),
                     .type = QStringLiteral("standard"),
                     .enabled = true,
                     .visible = true,
                     .toggle_state = -1};
  DbusMenuItem parent{.id = 1,
                      .label = QStringLiteral("Parent"),
                      .type = QStringLiteral("standard"),
                      .enabled = true,
                      .visible = true,
                      .toggle_state = -1,
                      .children = {child}};

  DbusMenuModel model({parent});
  EXPECT_TRUE(model.data(model.index(0, 0), DbusMenuModel::HasSubmenuRole).toBool());

  DbusMenuModel* sub = model.submenuAt(0);
  ASSERT_NE(sub, nullptr);
  ASSERT_EQ(sub->rowCount(), 1);
  EXPECT_EQ(sub->data(sub->index(0, 0), DbusMenuModel::IdRole).toInt(), 10);

  // Second call returns the cached instance.
  EXPECT_EQ(model.submenuAt(0), sub);
}

TEST(DbusMenuModel, SubmenuAtReturnsNullForNoChildren) {
  DbusMenuItem leaf{.id = 1,
                    .label = QStringLiteral("Leaf"),
                    .type = QStringLiteral("standard"),
                    .enabled = true,
                    .visible = true,
                    .toggle_state = -1};
  DbusMenuModel model({leaf});
  EXPECT_EQ(model.submenuAt(0), nullptr);
}

TEST(DbusMenuModel, LabelRoleStripsLeadingUnderscore) {
  DbusMenuItem item{.id = 1,
                    .label = QStringLiteral("_File"),
                    .type = QStringLiteral("standard"),
                    .enabled = true,
                    .visible = true,
                    .toggle_state = -1};
  DbusMenuModel model({item});
  EXPECT_EQ(model.data(model.index(0, 0), DbusMenuModel::LabelRole).toString(), QStringLiteral("File"));
}
