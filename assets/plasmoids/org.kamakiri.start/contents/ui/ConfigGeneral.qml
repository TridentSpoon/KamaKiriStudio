// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.iconthemes as KIconThemes
Kirigami.FormLayout {
    id: page
    property string cfg_icon: ""
    property string cfg_look: "fluent11"
    property string cfg_mode: "desktop"
    KIconThemes.IconDialog {
        id: iconDialog
        onIconNameChanged: if (iconName.length > 0) page.cfg_icon = iconName
    }
    RowLayout {
        Kirigami.FormData.label: i18n("Button icon:")
        Kirigami.Icon { source: page.cfg_icon || "view-app-grid"; implicitWidth: 32; implicitHeight: 32 }
        Button { text: i18n("Choose icon…"); onClicked: iconDialog.open() }
        Button { text: i18n("Use theme default"); enabled: page.cfg_icon.length > 0; onClicked: page.cfg_icon = "" }
    }
    Label { text: i18n("The icon chooser also lets you browse for your own image."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
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
