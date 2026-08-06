pragma ComponentBehavior: Bound

import QtQuick
import Holonight.Core
import Holonight.Controls
import HolonightShell

HnListDelegate {
    id: root

    required property bool isBestMatch
    required property string appName
    required property string appSubtitle
    required property string appIconName
    required property string appDesktopFile

    signal activated()

    objectName: "launcherResultRow"
    title: root.appName
    subtitle: root.appSubtitle
    selectionStyle: HnListDelegate.Outline
    sizeRole: root.isBestMatch ? HnControlSize.Hero : HnControlSize.Large
    dividerVisible: !root.selected && !root.isBestMatch
    implicitHeight: root.isBestMatch ? 76 : 60
    onClicked: root.activated()

    leadingContent: Component {
        Image {
            id: icon

            property bool usedFallback: false

            width: root.isBestMatch ? 44 : 32
            height: width
            sourceSize: Qt.size(width, height)
            source: "image://icon/" + (icon.usedFallback || root.appIconName.length === 0
                ? "application-x-executable"
                : root.appIconName)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            onStatusChanged: {
                if (status === Image.Error && !icon.usedFallback)
                    icon.usedFallback = true
            }
        }
    }

    trailingContent: root.selected ? enterHintComponent : null

    Component {
        id: enterHintComponent

        HnKeyHint {
            text: qsTr("Enter")
        }
    }
}
