pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../WeatherIcon"

Item {
    id: root
    implicitWidth: 220 + 16 + temperatureMetrics.advanceWidth + tempUnitText.implicitWidth + 6
    TextMetrics {
        id: temperatureMetrics
        font: tempText.font
        text: "88"
    }
    implicitHeight: Math.max(256, currentContent.implicitHeight + 44)

    readonly property color dividerColor: HoloniightPalette.borderPassive
    WeatherDaylight { id: daylight }
    property bool testing: false
    property int previewIndex: 0
    readonly property var preview: WeatherIconBridge.previewCatalogue[root.previewIndex]

    function temperatureColor(temperature) {
        if (temperature <= 5) {
            return HoloniightPalette.accentCyan
        }
        if (temperature >= 30) {
            return HoloniightPalette.warning
        }
        return HoloniightPalette.textPrimary
    }

    Row {
        id: layoutRow
        anchors.fill: parent
        spacing: 16

        Item {
            id: iconColumn
            width: Math.min(240, Math.max(0, (root.width - layoutRow.spacing) * 0.46))
            height: parent.height

            WeatherIcon {
                id: heroIcon
                objectName: "weatherHeroIcon"
                provider: root.testing ? root.preview.provider : WeatherService.current.provider
                conditionCode: root.testing ? root.preview.code : (WeatherService.hasData && WeatherService.current.availableFields.includes("conditionId") ? WeatherService.current.conditionId : -1)
                isDay: root.testing ? root.preview.isDay : daylight.isDay
                daylightAvailable: root.testing || daylight.available
                iconSize: Math.min(220, parent.width, parent.height)
                anchors.centerIn: parent
            }
            Loader {
                anchors.fill: parent
                active: root.testing
                sourceComponent: Item {
                    Text {
                        objectName: "weatherPreviewCaption"
                        anchors.bottom: parent.bottom
                        width: parent.width
                        text: qsTr("%1 · %2 · %3\n%4").arg(root.preview.providerName).arg(root.preview.code).arg(root.preview.variant).arg(root.preview.description)
                        color: HoloniightPalette.textPrimary
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

        }

        Item {
            width: Math.max(0, root.width - iconColumn.width - layoutRow.spacing)
            height: parent.height

            Column {
                id: currentContent
                width: parent.width
                spacing: 6

                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -22

                Row {
                    id: temperatureRow
                    visible: WeatherService.current.availableFields.includes("temperature")
                    width: parent.width
                    spacing: 6

                    Text {
                        id: tempText
                        objectName: "weatherTemperature"
                        width: Math.min(implicitWidth, Math.max(0, temperatureRow.width - tempUnitText.implicitWidth - temperatureRow.spacing))
                        text: WeatherService.hasData ? Math.round(WeatherService.displayTemperature(WeatherService.current.temperature, WeatherService.temperatureUnit)) : ""
                        color: WeatherService.hasData ? root.temperatureColor(WeatherService.current.temperature) : HoloniightPalette.textPrimary
                        font.pointSize: AppearanceService.displayFontSize * 3.5
                        font.family: AppearanceService.displayFont
                        font.weight: Font.Thin
                        elide: Text.ElideRight
                    }

                    Text {
                        id: tempUnitText
                        objectName: "weatherTemperatureUnit"
                        text: WeatherService.hasData ? WeatherService.temperatureUnit : ""
                        color: tempText.color
                        font.pointSize: AppearanceService.displayFontSize * 1.75
                        font.family: AppearanceService.displayFont
                        font.weight: Font.Thin

                        anchors.top: tempText.top
                        anchors.topMargin: 8
                    }
                }

                HnLabel {
                    width: parent.width
                    visible: WeatherService.current.availableFields.includes("condition")
                    objectName: "weatherCondition"
                    rawText: WeatherService.hasData ? WeatherService.current.condition : ""
                    role: HnTypographyRole.MicroHeader
                    color: HoloniightPalette.accentViolet
                    font.family: AppearanceService.uiFont
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }

                Item {
                    width: parent.width
                    visible: WeatherService.current.availableFields.includes("feelsLike") && WeatherService.showFeelsLike
                    height: 12

                    HnSeparator {
                        orientation: Qt.Horizontal
                        width: parent.width
                        fadeMode: HnSeparator.FadeEnd
                        opacity: 0.5
                    }
                }

                HnLabel {
                    width: parent.width
                    objectName: "weatherFeelsLike"
                    visible: WeatherService.current.availableFields.includes("feelsLike") && WeatherService.showFeelsLike
                    rawText: WeatherService.hasData
                        ? "Feels like " + WeatherService.formatTemperature(WeatherService.current.feelsLike, WeatherService.temperatureUnit)
                        : ""
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textPrimary
                    elide: Text.ElideRight
                }
            }
        }
    }
}
