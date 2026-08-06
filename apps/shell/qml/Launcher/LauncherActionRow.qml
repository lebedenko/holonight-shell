import Holonight.Core
import Holonight.Controls

HnListDelegate {
    id: root

    required property string actionName
    required property string parentAppName
    required property string actionExec
    signal activated()

    objectName: "launcherActionRow"
    title: root.actionName
    subtitle: root.parentAppName
    selectionStyle: HnListDelegate.Outline
    sizeRole: HnControlSize.Large
    dividerVisible: true
    implicitHeight: 60
    onClicked: root.activated()
}
