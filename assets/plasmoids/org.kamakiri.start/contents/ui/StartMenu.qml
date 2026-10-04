// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.kicker 0.1 as Kicker
import org.kde.plasma.private.sessions 2.0 as Sessions
Rectangle {
    id: menu
    property string look: "fluent11"
    property string appearance: "desktop"
    property bool preview: false
    property var appletInterface: null
    property var favoriteIds: ["org.kde.dolphin.desktop", "systemsettings.desktop", "org.kde.konsole.desktop", "org.kde.kate.desktop"]
    property alias searchText: search.text
    property bool allApps: false
    property var catalog: rootCatalog.count > 0 ? rootCatalog.modelForRow(0) : null
    readonly property int applicationCount: catalog ? catalog.count : 0
    readonly property int favoriteCount: favorites.count
    property color base: appearance === "light" ? "#f3f3f3" : appearance === "dark" ? "#202020" : Kirigami.Theme.backgroundColor
    property color ink: appearance === "light" ? "#191919" : appearance === "dark" ? "#f4f4f4" : Kirigami.Theme.textColor
    property color accentColor: Kirigami.Theme.highlightColor
    property color tileColor: Qt.tint(base, Qt.rgba(ink.r, ink.g, ink.b, 0.06))
    signal dismiss()
    signal favoritesEdited(var ids)
    implicitWidth: look === "fluent10" ? 760 : 640
    implicitHeight: 650
    Layout.minimumWidth: implicitWidth
    Layout.minimumHeight: implicitHeight
    color: base
    radius: look === "fluent10" ? 0 : look === "fluent11" ? 8 : 24
    border.color: Qt.rgba(ink.r, ink.g, ink.b, 0.13)
    function focusSearch() { search.forceActiveFocus(); }
    function run(model, row) { if (!preview && model.trigger(row, "", null)) dismiss(); }
    function pin(id) {
        if (preview) return;
        var ids = favoriteIds.slice(); var at = ids.indexOf(id);
        if (at < 0) ids.push(id); else ids.splice(at, 1);
        favoriteIds = ids; favoritesEdited(ids);
    }
    Kicker.RootModel { id: rootCatalog; autoPopulate: true; flat: true; sorted: true; appletInterface: menu.appletInterface || menu; appNameFormat: 0; showAllApps: true; showAllAppsCategorized: false; showRecentApps: false; showRecentDocs: false; showRecentFolders: false; showFavoritesPlaceholder: false; showPowerSession: false; showRootSeparator: false }
    Kicker.SimpleFavoritesModel { id: favorites; favorites: menu.favoriteIds }
    Kicker.SimpleFavoritesModel { id: settingsApp; favorites: ["systemsettings.desktop"] }
    Kicker.RecentUsageModel { id: recent; shownItems: Kicker.RecentUsageModel.OnlyApps; ordering: Kicker.RecentUsageModel.Recent }
    Sessions.SessionManagement { id: session }
    Keys.onEscapePressed: dismiss()
    ColumnLayout {
        anchors.fill: parent; anchors.margins: menu.look === "fluent10" ? 16 : 28; spacing: 16
        TextField {
            id: search; Layout.fillWidth: true; Layout.preferredHeight: 42
            placeholderText: "Search for apps"; placeholderTextColor: Qt.rgba(menu.ink.r,menu.ink.g,menu.ink.b,.65); color: menu.ink; selectByMouse: true
            background: Rectangle { color: menu.tileColor; radius: menu.look === "fluent10" ? 0 : 6; border.color: menu.accentColor; border.width: search.activeFocus ? 1 : 0 }
            onAccepted: {
                if (!text.trim().length) return;
                for (var i = 0; i < menu.applicationCount; ++i) if (menu.catalog.labelForRow(i).toLowerCase().indexOf(text.toLowerCase()) >= 0) { menu.run(menu.catalog, i); break; }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: search.text.length ? "Search results" : menu.allApps || menu.look === "fluent10" ? "All apps" : "Pinned"; color: menu.ink; font.bold: true; Layout.fillWidth: true }
            Button { palette.buttonText: menu.ink; palette.button: menu.tileColor; text: menu.allApps ? "‹ Back" : "All apps ›"; visible: menu.look !== "fluent10" && !search.text.length; onClicked: menu.allApps = !menu.allApps }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 20
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: menu.look === "fluent10" ? 270 : 0; Layout.preferredWidth: 300
                visible: menu.look === "fluent10" || menu.allApps || search.text.length > 0
                ListView {
                    id: list; clip: true; model: menu.catalog
                    delegate: ItemDelegate {
                        required property int index
                        required property var model
                        width: list.width
                        property bool matches: (!menu.preview || String(model.favoriteId).indexOf("org.kde.") >= 0 || String(model.favoriteId).indexOf("systemsettings") >= 0) && (!search.text.length || String(model.display).toLowerCase().indexOf(search.text.toLowerCase()) >= 0)
                        height: matches ? 46 : 0; visible: matches
                        contentItem: RowLayout {
                            Kirigami.Icon { source: model.decoration; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                            Label { text: model.display; color: menu.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                            ToolButton { enabled: !menu.preview; palette.buttonText: menu.ink; text: favorites.isFavorite(model.favoriteId) ? "−" : "+"; Accessible.name: "Pin or unpin " + model.display; onClicked: menu.pin(model.favoriteId) }
                        }
                        background: Rectangle { color: parent.hovered ? menu.tileColor : "transparent"; radius: menu.look === "fluent10" ? 0 : 6 }
                        onClicked: menu.run(menu.catalog, index)
                    }
                }
            }
            ColumnLayout {
                visible: !search.text.length && (!menu.allApps || menu.look === "fluent10")
                Layout.fillWidth: true; Layout.fillHeight: true
                Label { visible: menu.look === "fluent10"; text: "Pinned tiles"; color: menu.ink; font.bold: true }
                GridView {
                    id: pins; model: favorites; Layout.fillWidth: true; Layout.fillHeight: menu.look !== "fluent11"; Layout.preferredHeight: menu.look === "fluent11" ? 282 : -1; clip: true
                    cellWidth: width / (menu.look === "fluent10" ? 3 : 6); cellHeight: menu.look === "fluent10" ? 110 : 94
                    delegate: ItemDelegate {
                        required property int index
                        required property var model
                        width: pins.cellWidth - 4; height: pins.cellHeight - 4
                        contentItem: ColumnLayout {
                            Kirigami.Icon { source: model.decoration; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 32; Layout.preferredHeight: 32 }
                            Label { text: model.display; color: menu.look === "fluent10" ? "white" : menu.ink; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        background: Rectangle { color: menu.look === "fluent10" ? menu.accentColor : parent.hovered ? menu.tileColor : "transparent"; radius: menu.look === "fluent10" ? 0 : 6 }
                        onClicked: menu.run(favorites, index)
                    }
                }
                Label { visible: menu.look === "fluent11"; text: "Recommended"; color: menu.ink; font.bold: true }
                GridView {
                    id: recommended; visible: menu.look === "fluent11"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: menu.preview ? favorites : recent; cellWidth: width / 2; cellHeight: 52
                    delegate: ItemDelegate {
                        required property int index
                        required property var model
                        width: recommended.cellWidth; height: recommended.cellHeight
                        contentItem: RowLayout {
                            Kirigami.Icon { source: model.decoration; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                            ColumnLayout { Label { text: model.display; color: menu.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: menu.preview ? "Preview application" : "Recently used"; color: menu.ink; opacity: .6; font.pixelSize: 11 }
                            }
                        }
                        background: Rectangle { color: parent.hovered ? menu.tileColor : "transparent"; radius: 6 }
                        onClicked: menu.run(recommended.model,index)
                    }
                }
                Label { visible: menu.look === "fluent11" && !menu.preview && recent.count === 0; text: "Your recently used applications will appear here."; color: menu.ink; opacity: .65; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: "Pin applications using + in All apps."; color: menu.ink; opacity: .65; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: menu.ink; opacity: .12 }
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Icon { source: "user-identity"; color: menu.ink; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
            Label { text: "Your desktop"; color: menu.ink; Layout.fillWidth: true }
            Button { palette.buttonText: menu.ink; palette.button: menu.tileColor; text: "Settings"; enabled: !menu.preview; onClicked: menu.run(settingsApp,0) }
            ToolButton { palette.buttonText: menu.ink; text: "⏻"; Accessible.name: "Power and session"; enabled: !menu.preview; onClicked: power.open()
                Menu { id: power
                    MenuItem { text: "Lock"; onTriggered: session.lock() }
                    MenuItem { text: "Sign out…"; onTriggered: session.requestLogoutPrompt() }
                    MenuItem { text: "Restart…"; onTriggered: session.requestReboot(Sessions.SessionManagement.ForcePrompt) }
                    MenuItem { text: "Shut down…"; onTriggered: session.requestShutdown(Sessions.SessionManagement.ForcePrompt) }
                }
            }
        }
    }
    Component.onCompleted: focusSearch()
}
