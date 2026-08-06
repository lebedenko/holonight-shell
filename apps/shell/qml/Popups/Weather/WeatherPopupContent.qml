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

    component SectionLabel: Text {
        color: HoloniightPalette.accentBlue
        font.family: AppearanceService.titleFont
        font.pixelSize: AppearanceService.titleFontSize
        font.capitalization: Font.AllUppercase
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
                    text: "Current Weather"
                    width: parent.width
                }

                Text {
                    objectName: "weatherLocationLabel"
                    width: parent.width
                    visible: text.length > 0
                    text: WeatherService.locationLabel
                    color: HoloniightPalette.textSecondary
                    font.family: AppearanceService.uiFont
                    font.pixelSize: AppearanceService.titleFontSize
                    elide: Text.ElideRight
                }

                RowLayout {
                    width: parent.width
                    height: 256
                    spacing: 24

                    WeatherCurrentSection {
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
                text: "Hourly Forecast"
                width: parent.width
            }

            WeatherHourlyStrip {
                width: parent.width
                height: 148
            }

            HnSeparator {
                orientation: Qt.Horizontal
                width: parent.width
                fadeMode: HnSeparator.FadeBoth
                opacity: 0.5
            }

            RowLayout {
                width: parent.width
                height: 210
                spacing: 18

                Column {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 455
                    Layout.alignment: Qt.AlignTop
                    spacing: 8

                    SectionLabel {
                        text: "Forecast Summary"
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
                    Layout.preferredWidth: 230
                    Layout.alignment: Qt.AlignTop
                    spacing: 10

                    SectionLabel {
                        text: "Details"
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
                                Text {
                                    text: "SUNRISE"
                                    color: HoloniightPalette.textSecondary
                                    font.pixelSize: 9
                                    font.family: AppearanceService.uiFont
                                }
                                Text {
                                    text: WeatherService.hasData
                                        ? Qt.formatTime(new Date(WeatherService.current.sunrise * 1000), "HH:mm")
                                        : "—"
                                    color: HoloniightPalette.textPrimary
                                    font.pixelSize: 13
                                    font.family: AppearanceService.uiFont
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
                                Text {
                                    text: "SUNSET"
                                    color: HoloniightPalette.textSecondary
                                    font.pixelSize: 9
                                    font.family: AppearanceService.uiFont
                                }
                                Text {
                                    text: WeatherService.hasData
                                        ? Qt.formatTime(new Date(WeatherService.current.sunset * 1000), "HH:mm")
                                        : "—"
                                    color: HoloniightPalette.textPrimary
                                    font.pixelSize: 13
                                    font.family: AppearanceService.uiFont
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
                            Text {
                                text: "MOON"
                                color: HoloniightPalette.textSecondary
                                font.pixelSize: 9
                                font.family: AppearanceService.uiFont
                            }
                            Text {
                                text: {
                                    var mp = (WeatherService.hasData && WeatherService.daily.length > 0)
                                        ? WeatherService.daily[0].moonPhase
                                        : undefined;
                                    return WeatherIconBridge.moonPhaseDescription(mp);
                                }
                                color: HoloniightPalette.textPrimary
                                font.pixelSize: 13
                                font.family: AppearanceService.uiFont
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
                            Text {
                                text: "AIR QUALITY"
                                color: HoloniightPalette.textSecondary
                                font.pixelSize: 9
                                font.family: AppearanceService.uiFont
                            }
                            Text {
                                text: {
                                    if (!WeatherService.hasData) return "Not available"
                                    const aqi = WeatherService.current.aqi
                                    if (aqi >= 1 && aqi <= 5) {
                                        return airQualityGauge.labelForAqi(aqi) + " (" + aqi + ")"
                                    }
                                    return "Not available"
                                }
                                color: HoloniightPalette.textPrimary
                                font.pixelSize: 13
                                font.family: AppearanceService.uiFont
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
                height: 152
                spacing: 16

                Column {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    spacing: 4

                    SectionLabel {
                        text: "Temperature (°C)"
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
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    spacing: 4

                    SectionLabel {
                        text: "Precipitation (mm)"
                    }
                    PrecipitationGraph {
                        width: parent.width
                        height: 128
                    }
                }
            }

            RowLayout {
                width: parent.width
                height: 20

                Text {
                    Layout.fillWidth: true
                    text: "Updated: " + root.updateTime()
                    color: HoloniightPalette.accentBlue
                    font.pixelSize: 11
                    font.family: AppearanceService.uiFont
                }

                Text {
                    text: "Source: OpenWeather"
                    color: HoloniightPalette.accentBlue
                    font.pixelSize: 11
                    font.family: AppearanceService.uiFont
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }
}
