import QtQuick
import HolonightShell
import "../Utility" as Utility

// Root item for every desktop-widget layer-shell surface. C++ (WidgetManager) sets the required
// properties via setInitialProperties and keeps `remainingText` updated as the countdown ticks. The
// widget body is chosen by `widgetType` so future widget types slot in alongside time-to-event.
Item {
    id: root

    Utility.ThemeReloadBridge {}

    required property string widgetType
    required property string barMonitorName
    // Type-specific fields are plain (not required): each widget type's C++ qmlSource() supplies only
    // its own subset via setInitialProperties, so the other type's fields keep their empty defaults
    // rather than triggering "required property not initialized" warnings on the inactive Loader.
    property string titleText: ""
    property string remainingText: ""
    property string deadlineLabelText: ""
    property string timeText: ""
    property string secondsText: ""
    property string dateText: ""

    Loader {
        anchors.fill: parent
        active: root.widgetType === "time-to-event"
        sourceComponent: timeToEventComponent
    }

    Loader {
        anchors.fill: parent
        active: root.widgetType === "clock"
        sourceComponent: clockComponent
    }

    Component {
        id: timeToEventComponent

        TimeToEventWidget {
            titleText: root.titleText
            remainingText: root.remainingText
            deadlineLabel: root.deadlineLabelText
        }
    }

    Component {
        id: clockComponent

        ClockWidget {
            timeText: root.timeText
            secondsText: root.secondsText
            dateText: root.dateText
        }
    }
}
