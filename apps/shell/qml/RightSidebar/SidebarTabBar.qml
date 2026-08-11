pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Effects
import Holonight.Core
import Holonight.Controls

import HolonightShell

Item {
    id: root

    property int currentTab: 0
    property bool active: false

    signal tabSelected(int index)

    width: 224

    readonly property int panelPadding: 8
    readonly property int titlePadding: 16
    readonly property int tabProfileGap: 12

    readonly property var tabMeta: [
        { iconName: "overview",       label: "Overview" },
        { iconName: "calendar",       label: "Calendar" },
        { iconName: "notifications",  label: "Notifications" },
        { iconName: "system",         label: "System" },
        { iconName: "applications",   label: "Applications" },
        { iconName: "settings",       label: "Settings" }
    ]

    // Brand Header at the top
    Item {
        id: brandHeader

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: root.titlePadding
        anchors.right: parent.right
        anchors.rightMargin: root.titlePadding
        height: 64

        HnAppTitle {
            objectName: "shellAppTitle"
            anchors.centerIn: parent
            applicationName: qsTr("Shell")
            iconSource: "qrc:/HolonightShell/holonight-shell.svg"
            iconTinted: false
        }
    }

    // Tab buttons column
    Column {
        id: tabColumn

        anchors.top: brandHeader.bottom
        anchors.topMargin: 12
        anchors.bottom: bottomDivider.top
        anchors.bottomMargin: root.tabProfileGap
        anchors.left: parent.left
        anchors.leftMargin: root.panelPadding
        anchors.right: parent.right
        anchors.rightMargin: root.panelPadding
        spacing: 4

        Repeater {
            model: root.tabMeta

            SidebarTabButton {
                required property var modelData
                required property int index

                tabIndex: index
                iconName: modelData.iconName
                label: modelData.label
                isActive: root.currentTab === index
                badgeCount: modelData.iconName === "notifications" ? NotificationService.unreadCount : 0
                width: parent.width

                onClicked: root.tabSelected(index)
            }
        }
    }

    // Horizontal divider above the user profile
    Rectangle {
        id: bottomDivider

        anchors.bottom: userProfile.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: HoloniightPalette.surface
        opacity: 0.4
    }

    // User profile section at the bottom
    Item {
        id: userProfile

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Rectangle {
            id: profileBg

            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            radius: 8
            color: profileMouseArea.containsMouse
                ? Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g, HoloniightPalette.surface.b, 0.25)
                : "transparent"

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            Rectangle {
                id: avatarContainer

                width: 36
                height: 36
                radius: 18
                color: "transparent"
                anchors.verticalCenter: parent.verticalCenter

                // Fallback gradient background when image is not loaded
                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    clip: true
                    visible: avatarImage.status !== Image.Ready
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: HoloniightPalette.accentViolet }
                        GradientStop { position: 1.0; color: HoloniightPalette.accentCyan }
                    }

                    // Procedural user silhouette fallback
                    Item {
                        anchors.fill: parent

                        Rectangle {
                            width: 14
                            height: 14
                            radius: 7
                            color: HoloniightPalette.surface
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 6
                        }

                        Rectangle {
                            width: 26
                            height: 18
                            radius: 9
                            color: HoloniightPalette.surface
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: -4
                        }
                    }
                }

                // Hidden source image for MultiEffect
                Image {
                    id: avatarImage
                    anchors.fill: parent
                    source: SystemInfoService.avatarPath !== "" ? "file://" + SystemInfoService.avatarPath : ""
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    visible: false
                    layer.enabled: true
                    layer.textureSize: Qt.size(width * (Screen.devicePixelRatio || 2), height * (Screen.devicePixelRatio || 2))
                }

                // Mask source for MultiEffect
                Rectangle {
                    id: avatarMask
                    anchors.fill: parent
                    radius: width / 2
                    color: "black"
                    visible: false
                    antialiasing: true
                    layer.enabled: true
                    layer.textureSize: Qt.size(width * (Screen.devicePixelRatio || 2), height * (Screen.devicePixelRatio || 2))
                    layer.samples: 4
                }

                // Apply MultiEffect to clip the image to the circular mask
                MultiEffect {
                    anchors.fill: parent
                    source: avatarImage
                    maskEnabled: true
                    maskSource: avatarMask
                    visible: avatarImage.status === Image.Ready
                }
            }

            Column {
                width: parent.width - avatarContainer.width - chevronCanvas.width - 24
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: SystemInfoService.realName !== ""
                        ? SystemInfoService.realName
                        : (SystemInfoService.userName !== "" ? SystemInfoService.userName : "Alex")
                    color: HoloniightPalette.textPrimary
                    font.pointSize: 9.75
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Row {
                    spacing: 6

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: HoloniightPalette.success
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Online"
                        color: HoloniightPalette.textSecondary
                        font.pointSize: 8.25
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Canvas {
                id: chevronCanvas

                width: 12
                height: 12
                anchors.verticalCenter: parent.verticalCenter

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.strokeStyle = HoloniightPalette.textSecondary
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.moveTo(2, 4)
                    ctx.lineTo(6, 8)
                    ctx.lineTo(10, 4)
                    ctx.stroke()
                }
            }
        }

        MouseArea {
            id: profileMouseArea

            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                sessionMenu.open()
            }
        }

        // Session controls dropdown menu
        Controls.Menu {
            id: sessionMenu

            y: -sessionMenu.height - 4
            x: 8
            width: 184

            background: Rectangle {
                color: HoloniightPalette.surface
                border.color: HoloniightPalette.accentViolet
                border.width: 1
                radius: 8
            }

            Controls.MenuItem {
                id: lockItem

                text: "Lock"
                contentItem: Text {
                    text: lockItem.text
                    color: HoloniightPalette.textPrimary
                    font.pointSize: 9.75
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: lockItem.hovered ? Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g, HoloniightPalette.textPrimary.b, 0.08) : "transparent"
                    radius: 4
                }
                onTriggered: SessionService.lockScreen()
            }

            Controls.MenuItem {
                id: logoutItem

                text: "Log out"
                contentItem: Text {
                    text: logoutItem.text
                    color: HoloniightPalette.textPrimary
                    font.pointSize: 9.75
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: logoutItem.hovered ? Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g, HoloniightPalette.textPrimary.b, 0.08) : "transparent"
                    radius: 4
                }
                onTriggered: SessionService.logout()
            }

            Controls.MenuItem {
                id: rebootItem

                text: "Reboot"
                contentItem: Text {
                    text: rebootItem.text
                    color: HoloniightPalette.textPrimary
                    font.pointSize: 9.75
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: rebootItem.hovered ? Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g, HoloniightPalette.textPrimary.b, 0.08) : "transparent"
                    radius: 4
                }
                onTriggered: SessionService.reboot()
            }

            Controls.MenuItem {
                id: shutdownItem

                text: "Shut down"
                contentItem: Text {
                    text: shutdownItem.text
                    color: HoloniightPalette.textPrimary
                    font.pointSize: 9.75
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: shutdownItem.hovered ? Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g, HoloniightPalette.textPrimary.b, 0.08) : "transparent"
                    radius: 4
                }
                onTriggered: SessionService.shutdown()
            }
        }
    }

    // Divider on the right of the entire tab bar
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: HoloniightPalette.borderPassive
        opacity: 0.5
    }
}
