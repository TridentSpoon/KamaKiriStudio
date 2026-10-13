// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.iconthemes as KIconThemes
import org.kde.coreaddons as KCoreAddons
Kirigami.FormLayout {
    id: page
    property string cfg_icon: ""
    property string cfg_look: "fluent11"
    property string cfg_mode: "desktop"
    KIconThemes.IconDialog {
        id: iconDialog
        onIconNameChanged: if (iconName.length > 0) page.cfg_icon = iconName
    }
    // Prefer the distribution's original standalone logo over icon-theme variants.
    Image {
        id: originalDistroLogo
        visible: false
        source: KCoreAddons.KOSRelease.logo ? (KCoreAddons.KOSRelease.logo.startsWith("/") ? "file://" + KCoreAddons.KOSRelease.logo : "file:///usr/share/icons/" + KCoreAddons.KOSRelease.logo + ".svg") : ""
    }
    readonly property string distroIcon: originalDistroLogo.status === Image.Ready ? originalDistroLogo.source.toString() : (KCoreAddons.KOSRelease.logo || "start-here")
    readonly property var buttonChoices: [
        {label: i18n("Windows"), icon: "kamakiri-windows"},
        {label: KCoreAddons.KOSRelease.prettyName || i18n("Distribution"), icon: distroIcon},
        {label: i18n("KDE"), icon: Qt.resolvedUrl("icons/kde.svg").toString()},
        {label: i18n("App grid"), icon: "view-grid"},
        {label: i18n("Menu"), icon: "application-menu"},
        {label: i18n("Computer"), icon: "computer"},
        {label: i18n("Launch"), icon: "system-run"},
        {label: i18n("Home"), icon: "go-home"}
    ]
    Item {
        Kirigami.FormData.label: i18n("Button icon:")
        Layout.fillWidth: true
        Layout.minimumWidth: 260
        implicitHeight: Math.ceil(page.buttonChoices.length / columns) * 110 + 44
        readonly property int columns: Math.max(1, Math.floor((width - 24) / 84))
        Repeater {
            model: page.buttonChoices
            delegate: Item {
                id: tile
                required property int index
                required property var modelData
                x: 12 + (index % parent.columns) * 84
                y: Math.floor(index / parent.columns) * 110 + (index % parent.columns % 2) * 44
                width: 108
                height: 100
                readonly property bool selected: page.cfg_icon === modelData.icon || (modelData.icon === "kamakiri-windows" && page.cfg_icon === "" && page.cfg_look !== "caelestia")
                Canvas {
                    id: hexagon
                    anchors.fill: parent
                    onPaint: {
                        const ctx = getContext("2d"); ctx.reset();
                        ctx.beginPath();
                        for (let corner = 0; corner < 6; ++corner) {
                            const angle = corner * Math.PI / 3;
                            const x = width / 2 + 51 * Math.cos(angle);
                            const y = height / 2 + 46 * Math.sin(angle);
                            if (corner === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                        }
                        ctx.closePath();
                        ctx.fillStyle = Kirigami.Theme.backgroundColor; ctx.fill();
                        ctx.strokeStyle = tile.selected || tile.activeFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor;
                        ctx.lineWidth = tile.selected ? 3 : 1; ctx.stroke();
                    }
                    Connections { target: page; function onCfg_iconChanged() { hexagon.requestPaint(); } }
                    Connections { target: Kirigami.Theme; function onColorsChanged() { hexagon.requestPaint(); } }
                }
                Kirigami.Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 23; width: 28; height: 28
                    source: tile.modelData.icon
                    fallback: "start-here"
                    visible: tile.modelData.icon !== "kamakiri-windows"
                }
                Grid {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 23; columns: 2; spacing: 2
                    visible: tile.modelData.icon === "kamakiri-windows"
                    Repeater { model: 4; Rectangle { width: 13; height: 13; color: Kirigami.Theme.highlightColor } }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 57; width: 82; horizontalAlignment: Text.AlignHCenter
                    text: tile.modelData.label; elide: Text.ElideRight
                }
                activeFocusOnTab: true
                onActiveFocusChanged: hexagon.requestPaint()
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: selected
                Accessible.onPressAction: page.cfg_icon = modelData.icon
                Keys.onSpacePressed: page.cfg_icon = modelData.icon
                Keys.onReturnPressed: page.cfg_icon = modelData.icon
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { tile.forceActiveFocus(); page.cfg_icon = tile.modelData.icon; }
                    ToolTip.visible: containsMouse
                    ToolTip.text: tile.modelData.label
                }
            }
        }
    }
    RowLayout {
        Kirigami.Icon { source: page.cfg_icon === "kamakiri-windows" ? "view-app-grid" : (page.cfg_icon || "view-app-grid"); implicitWidth: 32; implicitHeight: 32 }
        Button { text: i18n("Choose your own icon…"); onClicked: iconDialog.open() }
        Button { text: i18n("Use theme default"); enabled: page.cfg_icon.length > 0; onClicked: page.cfg_icon = "" }
    }
    Label { text: i18n("Select a hexagon, then Apply. You can also browse for your own image."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
    ComboBox {
        Kirigami.FormData.label: i18n("Menu style:")
        model: [{text: i18n("Kamakiri"), value: "caelestia"}, {text: i18n("Fluent 11"), value: "fluent11"}, {text: i18n("Fluent 10"), value: "fluent10"}]
        textRole: "text"
        currentIndex: Math.max(0, ["caelestia", "fluent11", "fluent10"].indexOf(page.cfg_look))
        onActivated: index => page.cfg_look = model[index].value
    }
    ComboBox {
        Kirigami.FormData.label: i18n("Colours:")
        model: [{text: i18n("Follow desktop"), value: "desktop"}, {text: i18n("Light"), value: "light"}, {text: i18n("Dark"), value: "dark"}]
        textRole: "text"
        currentIndex: Math.max(0, ["desktop", "light", "dark"].indexOf(page.cfg_mode))
        onActivated: index => page.cfg_mode = model[index].value
    }
}
