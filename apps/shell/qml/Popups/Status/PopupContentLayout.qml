import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import "PopupMetrics.js" as PopupMetrics

// Measurement is independent of the allocated viewport. Consumers can allocate
// the body height to independently scrolling sections instead of scrolling it here.
Item {
    id: root

    default property alias contentData: body.data
    property bool scrollBody: true
    property Component header
    property Component footer
    property real contentSpacing: PopupMetrics.sectionGap
    property real naturalWidth: body.implicitWidth
    readonly property Item headerItem: headerLoader.status === Loader.Ready ? headerLoader.item as Item : null
    readonly property Item footerItem: footerLoader.status === Loader.Ready ? footerLoader.item as Item : null
    readonly property alias viewportItem: viewport
    readonly property real headerExtent: headerLoader.item ? headerLoader.implicitHeight + root.contentSpacing : 0
    readonly property real footerExtent: footerLoader.item ? footerLoader.implicitHeight + root.contentSpacing : 0
    readonly property bool pinned: height >= headerExtent + footerExtent + 64

    implicitWidth: Math.max(naturalWidth, headerLoader.implicitWidth, footerLoader.implicitWidth)
    implicitHeight: headerExtent + body.implicitHeight + footerExtent

    data: [
        Loader {
            id: headerLoader
            parent: root.pinned ? root : viewport.contentItem
            width: root.width
            y: 0
            sourceComponent: root.header
        },
        Flickable {
            id: viewport
            objectName: "popupContentViewport"
            parent: root
            width: root.width
            y: root.pinned ? root.headerExtent : 0
            height: Math.max(0, root.height - (root.pinned ? root.headerExtent + root.footerExtent : 0))
            contentWidth: width
            contentHeight: root.scrollBody ? body.implicitHeight + (root.pinned ? 0 : root.headerExtent + root.footerExtent) : height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            interactive: root.scrollBody && contentHeight > height
            onContentHeightChanged: Qt.callLater(returnToBounds)
            Controls.ScrollBar.vertical: Controls.ScrollBar {}

            ColumnLayout {
                id: body
                y: root.pinned ? 0 : root.headerExtent
                width: viewport.width
                height: root.scrollBody ? implicitHeight : Math.max(0, viewport.height - (root.pinned ? 0 : root.headerExtent + root.footerExtent))
                spacing: root.contentSpacing
            }
        },
        Loader {
            id: footerLoader
            parent: root.pinned ? root : viewport.contentItem
            width: root.width
            y: root.pinned ? root.height - implicitHeight : root.headerExtent + body.height + root.contentSpacing
            sourceComponent: root.footer
        }
    ]
}
