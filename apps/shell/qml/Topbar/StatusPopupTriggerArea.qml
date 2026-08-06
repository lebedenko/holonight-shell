import QtQuick
import HolonightShell

// Drop-in click area for a status widget. Toggles that widget's status popup and exposes
// isActivePopup so the host widget can render a selected state. Coexists with the widget's
// existing HoverHandler and BarTooltipArea (those use hover, this uses click).
Item {
    id: root

    required property string popupId
    required property string barMonitorName

    // True while THIS widget's popup is the visible one. Bound to StatusPopupSurface only —
    // never PopupSurface, whose visibility tracks the unrelated session/clock popup.
    readonly property bool isActivePopup: StatusPopupSurface.popupVisible
                                          && StatusPopupSurface.activePopupId === root.popupId

    anchors.fill: parent

    TapHandler {
        // MouseArea.onClicked fired on any press+release inside the bounds regardless of movement
        // in between; the default DragThreshold policy cancels on minor pointer drift, so match the
        // old, more forgiving behavior explicitly.
        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: {
            TooltipSurface.hide()
            const globalPos = root.mapToGlobal(0, 0)
            StatusPopupSurface.toggle(root.popupId, root.barMonitorName,
                                      Math.round(globalPos.x), Math.round(root.width))
        }
    }
}
