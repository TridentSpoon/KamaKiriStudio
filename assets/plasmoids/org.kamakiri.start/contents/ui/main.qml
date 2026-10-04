// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami
PlasmoidItem {
    id: root
    preferredRepresentation: compactRepresentation
    Plasmoid.icon: "view-app-grid"
    compactRepresentation: Item {
        Layout.minimumWidth: Kirigami.Units.iconSizes.medium
        Layout.minimumHeight: Kirigami.Units.iconSizes.medium
        Rectangle { anchors.centerIn: parent; width: 24; height: 24; color: "transparent"
            Grid { anchors.fill: parent; columns: 2; spacing: 2
                Repeater { model: 4; Rectangle { width: 11; height: 11; radius: root.Plasmoid.configuration.look === "caelestia" ? 4 : 0; color: Kirigami.Theme.highlightColor } }
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
        Connections { target: root; function onExpandedChanged() { if (root.expanded) { start.searchText = ""; start.focusSearch(); } } }
    }
}
