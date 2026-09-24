pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../Topbar"

Item {
    id: root

    readonly property int sidePad: 10
    readonly property int topPad: 8
    readonly property color dividerColor: Qt.rgba(HoloniightPalette.borderPassive.r,
                                                  HoloniightPalette.borderPassive.g,
                                                  HoloniightPalette.borderPassive.b, 0.62)

    function updateTime() {
        if (!WeatherService.hasData || WeatherService.current.timeUpdated.length === 0) {
            return "—"
        }
        const parsed = new Date(WeatherService.current.timeUpdated)
        if (isNaN(parsed.getTime())) {
            return WeatherService.current.timeUpdated
        }
        return Qt.formatTime(parsed, "HH:mm")
    }

    component SectionLabel: HnLabel {
        role: HnTypographyRole.MicroHeader
        color: HoloniightPalette.accentBlue
        elide: Text.ElideRight
    }

    Flickable {
        id: viewport
        objectName: "weatherViewport"
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: stack.height + root.topPad * 2
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: contentHeight > height

        Column {
            id: stack
            x: root.sidePad
            y: root.topPad
            width: viewport.width - root.sidePad * 2
            spacing: 8

            Column {
                width: parent.width
                spacing: 6

                SectionLabel {
                    rawText: qsTr("Current Weather")
                    width: parent.width
                }

                HnLabel {
                    objectName: "weatherLocationLabel"
                    width: parent.width
                    visible: text.length > 0
                    rawText: WeatherService.locationLabel
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textSecondary
                    elide: Text.ElideRight
                }

                RowLayout {
                    width: parent.width
                    height: Math.max(256, currentSection.implicitHeight, detailsGrid.implicitHeight)
                    spacing: 24

                    WeatherCurrentSection {
                        id: currentSection
                        Layout.fillWidth: true
                        Layout.preferredWidth: 490
                        Layout.fillHeight: true
                    }

                    HnSeparator {
                        orientation: Qt.Vertical
                        implicitHeight: parent.height
                        fadeMode: HnSeparator.FadeBoth
                        opacity: 0.5
                    }

                    WeatherDetailsGrid {
                        id: detailsGrid
                        Layout.preferredWidth: 220
                        Layout.alignment: Qt.AlignTop
                    }
                }
            }

            HnSeparator {
                orientation: Qt.Horizontal
                width: parent.width
                fadeMode: HnSeparator.FadeBoth
                opacity: 0.5
            }

            SectionLabel {
                rawText: qsTr("Hourly Forecast")
                width: parent.width
            }

            WeatherHourlyStrip {
                width: parent.width
                height: implicitHeight
            }

            HnSeparator {
                orientation: Qt.Horizontal
                width: parent.width
                fadeMode: HnSeparator.FadeBoth
                opacity: 0.5
            }

            RowLayout {
                width: parent.width
                height: Math.max(210, forecastSummary.implicitHeight + 16, forecastDetails.implicitHeight + 16)
                spacing: 18

                Column {
                    id: forecastSummary
                    Layout.fillWidth: true
                    Layout.preferredWidth: 455
                    Layout.alignment: Qt.AlignTop
                    spacing: 8

                    SectionLabel {
                        rawText: qsTr("Forecast Summary")
                        width: parent.width
                    }

                    WeatherDailyCards {
                        width: parent.width
                    }
                }

                HnSeparator {
                    orientation: Qt.Vertical
                    implicitHeight: parent.height
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.5
                }

                Column {
                    id: forecastDetails
                    Layout.preferredWidth: 230
                    Layout.alignment: Qt.AlignTop
                    spacing: 10

                    SectionLabel {
                        rawText: qsTr("Details")
                        width: parent.width
                    }

                    // Line 1: Sunrise & Sunset
                    RowLayout {
                        width: parent.width
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Image {
                                source: "qrc:/HolonightShell/weather-png/512x512/sunrise.png"
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                sourceSize: Qt.size(48, 48)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }
                            Column {
                                Layout.fillWidth: true
                                spacing: 1
                                HnLabel {
                                    width: parent.width
                                    rawText: qsTr("Sunrise")
                                    role: HnTypographyRole.MicroHeader
                                    color: HoloniightPalette.textSecondary
                                    font.family: AppearanceService.uiFont
                                    elide: Text.ElideRight
                                }
                                HnLabel {
                                    width: parent.width
                                    rawText: WeatherService.hasData
                                        ? Qt.formatTime(new Date(WeatherService.current.sunrise * 1000), "HH:mm")
                                        : "—"
                                    role: HnTypographyRole.Caption
                                    color: HoloniightPalette.textPrimary
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Image {
                                source: "qrc:/HolonightShell/weather-png/512x512/sunset.png"
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                sourceSize: Qt.size(48, 48)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }
                            Column {
                                Layout.fillWidth: true
                                spacing: 1
                                HnLabel {
                                    width: parent.width
                                    rawText: qsTr("Sunset")
                                    role: HnTypographyRole.MicroHeader
                                    color: HoloniightPalette.textSecondary
                                    font.family: AppearanceService.uiFont
                                    elide: Text.ElideRight
                                }
                                HnLabel {
                                    width: parent.width
                                    rawText: WeatherService.hasData
                                        ? Qt.formatTime(new Date(WeatherService.current.sunset * 1000), "HH:mm")
                                        : "—"
                                    role: HnTypographyRole.Caption
                                    color: HoloniightPalette.textPrimary
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    HnSeparator {
                        orientation: Qt.Horizontal
                        width: parent.width
                        fadeMode: HnSeparator.FadeEnd
                        opacity: 0.5
                    }

                    // Line 2: Moon Phase
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        Image {
                            source: {
                                var mp = (WeatherService.hasData && WeatherService.daily.length > 0)
                                    ? WeatherService.daily[0].moonPhase
                                    : undefined;
                                return "qrc:/HolonightShell/weather-png/512x512/" + WeatherIconBridge.moonPhaseIcon(mp) + ".png";
                            }
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            sourceSize: Qt.size(48, 48)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            HnLabel {
                                width: parent.width
                                rawText: qsTr("Moon")
                                role: HnTypographyRole.MicroHeader
                                color: HoloniightPalette.textSecondary
                                font.family: AppearanceService.uiFont
                                elide: Text.ElideRight
                            }
                            HnLabel {
                                width: parent.width
                                rawText: {
                                    var mp = (WeatherService.hasData && WeatherService.daily.length > 0)
                                        ? WeatherService.daily[0].moonPhase
                                        : undefined;
                                    return WeatherIconBridge.moonPhaseDescription(mp);
                                }
                                role: HnTypographyRole.Caption
                                color: HoloniightPalette.textPrimary
                                elide: Text.ElideRight
                            }
                        }
                    }

                    HnSeparator {
                        orientation: Qt.Horizontal
                        width: parent.width
                        fadeMode: HnSeparator.FadeEnd
                        opacity: 0.5
                    }

                    // Line 3: Air Quality
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        WeatherAqiGauge {
                            id: airQualityGauge
                            aqi: WeatherService.hasData ? WeatherService.current.aqi : 0
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            HnLabel {
                                width: parent.width
                                rawText: qsTr("Air Quality")
                                role: HnTypographyRole.MicroHeader
                                color: HoloniightPalette.textSecondary
                                font.family: AppearanceService.uiFont
                                elide: Text.ElideRight
                            }
                            HnLabel {
                                width: parent.width
                                rawText: {
                                    if (!WeatherService.hasData) return "Not available"
                                    const aqi = WeatherService.current.aqi
                                    if (aqi >= 1 && aqi <= 5) {
                                        return airQualityGauge.labelForAqi(aqi) + " (" + aqi + ")"
                                    }
                                    return "Not available"
                                }
                                role: HnTypographyRole.Caption
                                color: HoloniightPalette.textPrimary
                                elide: Text.ElideRight
                            }
                        }
                    }

                }
            }

            HnSeparator {
                orientation: Qt.Horizontal
                width: parent.width
                fadeMode: HnSeparator.FadeBoth
                opacity: 0.5
            }

            RowLayout {
                width: parent.width
                height: Math.max(152, temperatureGraphColumn.implicitHeight, precipitationGraphColumn.implicitHeight)
                spacing: 16

                Column {
                    id: temperatureGraphColumn
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    spacing: 4

                    SectionLabel {
                        rawText: qsTr("Temperature (°C)")
                        width: parent.width
                    }
                    TemperatureGraph {
                        width: parent.width
                        height: 128
                    }
                }

                HnSeparator {
                    orientation: Qt.Vertical
                    implicitHeight: parent.height
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.32
                }

                Column {
                    id: precipitationGraphColumn
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    spacing: 4

                    SectionLabel {
                        rawText: qsTr("Precipitation (mm)")
                        width: parent.width
                    }
                    PrecipitationGraph {
                        width: parent.width
                        height: 128
                    }
                }
            }

            RowLayout {
                width: parent.width
                height: Math.max(20, updatedLabel.implicitHeight, sourceLabel.implicitHeight)

                HnLabel {
                    id: updatedLabel
                    Layout.fillWidth: true
                    rawText: qsTr("Updated: %1").arg(root.updateTime())
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.accentBlue
                    elide: Text.ElideRight
                }

                HnLabel {
                    id: sourceLabel
                    Layout.maximumWidth: parent.width * 0.5
                    rawText: qsTr("Source: OpenWeather")
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.accentBlue
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }
    }
}
