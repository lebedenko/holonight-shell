import QtQuick
import Holonight.Core

Item {
    id: root

    property string iconName: ""
    property string pixmapUrl: ""
    property int iconSize: 24
    property color tintColor: HoloniightPalette.textSecondary
    property bool preferSemanticTint: true
    property string fallbackIconName: ""

    readonly property string effectiveIconName: root.iconName.length > 0 ? root.iconName : root.fallbackIconName
    readonly property bool hasNamedIcon: root.effectiveIconName.length > 0
    readonly property bool hasPixmap: root.pixmapUrl.length > 0
    readonly property bool usesSemanticTint: root.preferSemanticTint
                                         && root.hasNamedIcon
                                         && !root._isExactFileSource(root.effectiveIconName)
                                         && HnIconProvider["supportsSemanticColors"](root.effectiveIconName)
    readonly property url resolvedExactSource: {
        if (root._useFallbackAfterError && root.fallbackIconName.length > 0)
            return root._exactSource(root.fallbackIconName)
        if (root.usesSemanticTint)
            return ""
        if (root.hasNamedIcon)
            return root._exactSource(root.effectiveIconName)
        return root.pixmapUrl
    }

    property bool _useFallbackAfterError: false

    implicitWidth: root.iconSize
    implicitHeight: root.iconSize
    Accessible.ignored: true

    function _isExactFileSource(source) {
        return source.startsWith("/") || source.startsWith("file://") || source.startsWith("qrc:/")
    }

    function _exactSource(source) {
        if (source.startsWith("/"))
            return "file://" + source
        if (source.startsWith("file://"))
            return source
        if (source.startsWith("qrc:/"))
            return source
        return "image://icon/" + source
    }

    onIconNameChanged: root._useFallbackAfterError = false
    onPixmapUrlChanged: root._useFallbackAfterError = false
    onFallbackIconNameChanged: root._useFallbackAfterError = false

    HnIcon {
        anchors.fill: parent
        visible: root.usesSemanticTint
        source: root.usesSemanticTint ? root.effectiveIconName : ""
        size: root.iconSize
        tinted: true
        normalColor: root.tintColor
    }

    Image {
        anchors.fill: parent
        visible: !root.usesSemanticTint && String(root.resolvedExactSource).length > 0
        sourceSize.width: root.iconSize
        sourceSize.height: root.iconSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        source: root.resolvedExactSource
        onStatusChanged: {
            if (status === Image.Error && !root._useFallbackAfterError
                    && root.iconName.length > 0 && root.fallbackIconName.length > 0) {
                root._useFallbackAfterError = true
            }
        }
    }
}
