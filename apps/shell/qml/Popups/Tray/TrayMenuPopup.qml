pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Holonight.Core
import Holonight.Controls
import HolonightShell
import "../../Tray"

import "../../Utility" as Utility

Item {
    id: root

    Utility.AppearanceReloadBridge {}

    // Injected via QQmlContext from TrayMenuSurface.
    property var menuModel: null
    property var menuClient: null
    property int depth: 0
    property int columnWidth: 244
    property int columnGap: 6
    property int columnCount: root.depth === 0 && root.menuClient ? root.menuClient.columnCount : 1
    property int columnIndex: root.depth === 0 && root.menuClient ? root.menuClient.columnIndex : 0
    property int columnStep: 1
    property real requestedPanelY: 0
    property var activeSubmenuModel: null
    property real activeSubmenuAnchorY: 0
    property int paddingLeft: root.depth === 0 && root.menuClient ? root.menuClient.paddingLeft : 0
    property int paddingRight: root.depth === 0 && root.menuClient ? root.menuClient.paddingRight : 0
    property int paddingTop: root.depth === 0 && root.menuClient ? root.menuClient.paddingTop : 0
    property int paddingBottom: root.depth === 0 && root.menuClient ? root.menuClient.paddingBottom : 0
    property int maxPanelHeight: root.depth === 0 && root.menuClient ? root.menuClient.maxPanelHeight : 480
    readonly property int menuMinHeight: 40
    readonly property real naturalPanelHeight: root.depth === 0 ? root.maxPanelHeight : Math.max(root.menuMinHeight, Math.min(menuList.contentHeight + 16, root.maxPanelHeight))
    readonly property real clampedPanelY: Math.max(0, Math.min(root.requestedPanelY - root.paddingTop,
                                                               Math.max(0, root.maxPanelHeight - root.naturalPanelHeight)))
    readonly property int nextColumnIndex: {
        var next = root.columnIndex + root.columnStep
        return next >= 0 && next < root.columnCount ? next : root.columnIndex
    }

    property alias panelItem: panel

    onActiveSubmenuModelChanged: {
        root.syncSubmenu()
        if (root.menuClient) {
            root.menuClient.setSubmenuModel(root.activeSubmenuModel)
        }
    }

    Component.onCompleted: {
        if (root.menuClient) {
            root.menuClient.setSubmenuModel(root.activeSubmenuModel)
        }
    }

    function syncSubmenu() {
        if (!submenuLoader.item) { return }
        submenuLoader.item.menuModel = root.activeSubmenuModel
        submenuLoader.item.menuClient = root.menuClient
        submenuLoader.item.depth = root.depth + 1
        submenuLoader.item.columnWidth = root.columnWidth
        submenuLoader.item.columnGap = root.columnGap
        submenuLoader.item.columnCount = root.columnCount
        submenuLoader.item.columnIndex = root.nextColumnIndex
        submenuLoader.item.columnStep = root.columnStep
        submenuLoader.item.requestedPanelY = root.activeSubmenuAnchorY - 8
        submenuLoader.item.paddingLeft = root.paddingLeft
        submenuLoader.item.paddingRight = root.paddingRight
        submenuLoader.item.paddingTop = root.paddingTop
        submenuLoader.item.paddingBottom = root.paddingBottom
        submenuLoader.item.maxPanelHeight = root.maxPanelHeight
    }

    implicitWidth: (root.columnWidth * root.columnCount) + ((root.columnCount - 1) * root.columnGap)
    implicitHeight: root.naturalPanelHeight
    focus: true

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "red"
        border.width: 1
        visible: AppearanceService.debugOverlays
        z: 99999
    }

    MultiEffect {
        source: panel
        anchors.fill: panel
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.5
        shadowOpacity: 0.12
        shadowScale: 1.01
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    HnSurfaceFrame {
        id: panel
        x: root.paddingLeft + (root.columnIndex * (root.columnWidth + root.columnGap))
        y: root.paddingTop + root.clampedPanelY
        width: root.columnWidth
        height: root.naturalPanelHeight
        surfaceRole: HnSurfaceRole.Menu
        fillColor: HoloniightPalette.surfaceRaised
        borderColor: HoloniightPalette.borderPassive
        borderWidth: HoloniightPalette.borderWidth
    }

    ListView {
        id: menuList
        anchors {
            fill: panel
            margins: 8
        }
        model: root.menuModel
        clip: true
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        spacing: 1



        // Repeater auto-fills all required properties from model roles.
        // Model role names match TrayMenuItem required property names exactly.
        delegate: TrayMenuItem {
            required property int index

            width: menuList.width

            onClicked: {
                if (root.menuClient) {
                    root.menuClient.activateItem(itemId)
                }
            }

            onSubmenuRequested: (anchorY) => {
                root.activeSubmenuAnchorY = anchorY
                root.activeSubmenuModel = root.menuModel ? root.menuModel.submenuAt(index) : null
            }
        }
    }

    Loader {
        id: submenuLoader
        anchors.fill: parent
        active: root.activeSubmenuModel !== null
        source: "qrc:/HolonightShell/Popups/Tray/TrayMenuPopup.qml"
        onLoaded: root.syncSubmenu()
    }

    Keys.onEscapePressed: {
        if (root.menuClient) {
            root.menuClient.close()
        }
    }
}
