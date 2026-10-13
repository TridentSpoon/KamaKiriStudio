// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami
PlasmoidItem {
    id: root
    readonly property string buttonIcon: root.Plasmoid.configuration.icon || ""
    preferredRepresentation: compactRepresentation
    // Keep the popup midpoint anchored to the Windows logo as tasks move.
    Item {
        id: centeredPopupAnchor
        parent: root.compactRepresentationItem ? root.compactRepresentationItem : root
        width: 24
        height: 24
        anchors.centerIn: parent
    }
    Timer {
        id: positionPopup
        interval: 0
        onTriggered: {
            if (!root.expanded || root.Plasmoid.configuration.look !== "fluent11") return;
            const popup = root.fullRepresentationItem ? root.fullRepresentationItem.Window.window : null;
            if (popup && popup.visualParent !== undefined) popup.visualParent = centeredPopupAnchor;
        }
    }
    Plasmoid.icon: root.buttonIcon === "kamakiri-windows" ? "view-app-grid" : (root.buttonIcon || "view-app-grid")
    compactRepresentation: Item {
        id: startButton
        Layout.minimumWidth: Kirigami.Units.iconSizes.medium
        Layout.minimumHeight: Kirigami.Units.iconSizes.medium
        Kirigami.Icon { anchors.centerIn: parent; width: 24; height: 24; source: root.buttonIcon === "kamakiri-windows" ? "" : root.buttonIcon; visible: root.buttonIcon.length > 0 && root.buttonIcon !== "kamakiri-windows" }
        Rectangle { visible: root.buttonIcon.length === 0 || root.buttonIcon === "kamakiri-windows"; anchors.centerIn: parent; width: 24; height: 24; color: "transparent"
            Grid { anchors.fill: parent; columns: 2; spacing: 2
                Repeater { model: 4; Rectangle { width: 11; height: 11; radius: root.buttonIcon !== "kamakiri-windows" && root.Plasmoid.configuration.look === "caelestia" ? 4 : 0; color: Kirigami.Theme.highlightColor } }
            }
        }
        MouseArea { anchors.fill: parent; onClicked: root.expanded = !root.expanded }
        Accessible.role: Accessible.Button
        Accessible.name: "Start"
        Accessible.onPressAction: root.expanded = !root.expanded
    }
    fullRepresentation: StartMenu {
        id: start
        look: root.Plasmoid.configuration.look
        appearance: root.Plasmoid.configuration.mode
        favoriteIds: root.Plasmoid.configuration.favorites
        appletInterface: root
        onFavoritesEdited: ids => root.Plasmoid.configuration.favorites = ids
        onDismiss: root.expanded = false
        Connections { target: root; function onExpandedChanged() { if (root.expanded) { start.searchText = ""; start.focusSearch(); positionPopup.restart(); } } }
    }
}
