import QtQuick

Item {
    required property string barMonitorName
    required property var contributionModel
    implicitWidth: contributionModel.contentWidth
    implicitHeight: 32
    visible: contributionModel.shown
}
