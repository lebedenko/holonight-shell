pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../Topbar"
import "../Status/PopupMetrics.js" as PopupMetrics

Item {
    id: root

    readonly property int sidePad: 0
    readonly property int topPad: 0
    readonly property color dividerColor: Qt.rgba(HoloniightPalette.borderPassive.r,
                                                  HoloniightPalette.borderPassive.g,
                                                  HoloniightPalette.borderPassive.b, 0.62)

    implicitWidth: Math.max(currentSection.implicitWidth + detailsGrid.naturalWidth + 2 * PopupMetrics.sectionGap + HnMetrics.separatorWidth, hourlyStrip.naturalWidth)
    implicitHeight: viewport.contentHeight
    readonly property bool narrow: width < implicitWidth

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

    component SectionLabel: PopupSectionLabel {}

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
            spacing: PopupMetrics.sectionGap

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

                GridLayout {
                    width: parent.width
                    height: implicitHeight
                    columns: root.narrow ? 1 : 3
                    columnSpacing: PopupMetrics.sectionGap
                    rowSpacing: PopupMetrics.sectionGap

                    WeatherCurrentSection {
                        id: currentSection
                        Layout.fillWidth: true
                        Layout.preferredWidth: currentSection.implicitWidth
                        Layout.fillHeight: true
                    }

                    HnSeparator {
                        orientation: Qt.Vertical
                    visible: !root.narrow
                    Layout.fillHeight: true
                            fadeMode: HnSeparator.FadeBoth
                        opacity: 0.5
                    }

                    WeatherDetailsGrid {
                        id: detailsGrid
                        Layout.preferredWidth: detailsGrid.naturalWidth
                        Layout.fillWidth: root.narrow
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
                id: hourlyStrip
                width: parent.width
                height: implicitHeight
            }

            HnSeparator {
                orientation: Qt.Horizontal
                width: parent.width
                fadeMode: HnSeparator.FadeBoth
                opacity: 0.5
            }

            GridLayout {
                width: parent.width
                height: implicitHeight
                columns: root.narrow ? 1 : 3
                columnSpacing: PopupMetrics.sectionGap
                rowSpacing: PopupMetrics.sectionGap

                Column {
                    id: forecastSummary
                    Layout.fillWidth: true
                    Layout.preferredWidth: 455
                    Layout.alignment: Qt.AlignTop
                    spacing: PopupMetrics.rowGap

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
                    visible: !root.narrow
                    Layout.fillHeight: true
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.5
                }

                Column {
                    id: forecastDetails
                    Layout.preferredWidth: detailsGrid.naturalWidth
                    Layout.fillWidth: root.narrow
                    Layout.alignment: Qt.AlignTop
                    spacing: 10

                    SectionLabel {
                        rawText: qsTr("Details")
                        width: parent.width
                    }

                    GridLayout {
                        objectName: "weatherSunEvents"
                        width: parent.width
                        columns: width >= sunrise.implicitWidth + sunset.implicitWidth + columnSpacing ? 2 : 1
                        columnSpacing: PopupMetrics.rowGap
                        rowSpacing: PopupMetrics.rowGap
                        WeatherSunEvent {
                            id: sunrise
                            objectName: "weatherSunrise"
                            Layout.fillWidth: true
                            title: qsTr("Sunrise")
                            iconSource: "qrc:/HolonightShell/weather-png/512x512/sunrise.png"
                            timeText: WeatherService.hasData ? Qt.formatTime(new Date(WeatherService.current.sunrise * 1000), "HH:mm") : "—"
                        }
                        WeatherSunEvent {
                            id: sunset
                            objectName: "weatherSunset"
                            Layout.fillWidth: true
                            title: qsTr("Sunset")
                            iconSource: "qrc:/HolonightShell/weather-png/512x512/sunset.png"
                            timeText: WeatherService.hasData ? Qt.formatTime(new Date(WeatherService.current.sunset * 1000), "HH:mm") : "—"
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

            GridLayout {
                width: parent.width
                height: implicitHeight
                columns: root.narrow ? 1 : 3
                columnSpacing: PopupMetrics.sectionGap
                rowSpacing: PopupMetrics.sectionGap

                Column {
                    id: temperatureGraphColumn
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    spacing: PopupMetrics.textGap

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
                    visible: !root.narrow
                    Layout.fillHeight: true
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.32
                }

                Column {
                    id: precipitationGraphColumn
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    spacing: PopupMetrics.textGap

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
