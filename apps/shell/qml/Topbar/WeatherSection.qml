import QtQuick
import HolonightShell

WeatherWidget {
    id: root

    visible: implicitWidth > 0
    enabled: root.ready
}
