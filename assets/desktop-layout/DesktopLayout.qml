// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: desktop
    property url wallpaper
    property color surface: "#14231c"
    property color foreground: "#d7e4d9"
    property color accent: "#509475"
    property int railWidth: 42
    property int frameWidth: 8
    property int cornerRadius: 24
    property bool showControls: true
    readonly property rect viewport: Qt.rect(railWidth, frameWidth, width - railWidth - frameWidth, height - frameWidth * 2)
    color: surface
    focus: true
    Accessible.name: "Kamakiri desktop layout preview"
    Keys.onEscapePressed: showControls = !showControls
    Rectangle {
        id: workspace
        x: desktop.viewport.x; y: desktop.viewport.y
        width: desktop.viewport.width; height: desktop.viewport.height
        color: Qt.darker(desktop.surface, 1.3)
        Image { anchors.fill: parent; source: desktop.wallpaper; fillMode: Image.PreserveAspectCrop }
        // Paint the four outside corners over the image; the wallpaper itself
        // needs no shader mask, including on the software/offscreen renderer.
        Repeater {
            model: 4
            Canvas {
                required property int index
                width: desktop.cornerRadius; height: width
                x: index === 1 || index === 2 ? workspace.width - width : 0
                y: index >= 2 ? workspace.height - height : 0
                rotation: index * 90
                onPaint: {
                    const c = getContext("2d"); c.reset();
                    c.beginPath(); c.moveTo(0, 0); c.lineTo(width, 0);
                    c.arc(width, height, width, -Math.PI / 2, -Math.PI, true);
                    c.closePath(); c.fillStyle = desktop.surface; c.fill();
                }
                onWidthChanged: requestPaint()
                Connections { target: desktop; function onSurfaceChanged() { requestPaint(); } }
            }
        }
        Rectangle {
            visible: desktop.showControls
            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 28
            width: Math.min(340, parent.width - 56); height: controls.implicitHeight + 36
            radius: 18; color: desktop.surface
            border.color: Qt.rgba(desktop.foreground.r, desktop.foreground.g, desktop.foreground.b, 0.15)
            ColumnLayout {
                id: controls
                anchors.fill: parent; anchors.margins: 18; spacing: 8
                Label { text: "Kamakiri rail and frame"; color: desktop.foreground; font.bold: true; font.pixelSize: 18 }
                Label { text: "Layout preview · desktop settings untouched"; color: desktop.foreground; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Rail · " + desktop.railWidth + " px"; color: desktop.foreground }
                Slider { Layout.fillWidth: true; from: 32; to: 64; stepSize: 2; value: desktop.railWidth; onMoved: desktop.railWidth = value; Accessible.name: "Rail width" }
                Label { text: "Frame · " + desktop.frameWidth + " px"; color: desktop.foreground }
                Slider { Layout.fillWidth: true; from: 4; to: 16; stepSize: 1; value: desktop.frameWidth; onMoved: desktop.frameWidth = value; Accessible.name: "Frame thickness" }
                Label { text: "Corners · " + desktop.cornerRadius + " px"; color: desktop.foreground }
                Slider { Layout.fillWidth: true; from: 8; to: 40; stepSize: 2; value: desktop.cornerRadius; onMoved: desktop.cornerRadius = value; Accessible.name: "Corner radius" }
                Label { text: "Click the rail’s settings button to hide these controls."; color: desktop.foreground; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
    }
    component RailGlyph: Canvas {
        id: glyph
        property int kind: 0
        onPaint: {
            const c = getContext("2d"); c.reset();
            c.strokeStyle = desktop.foreground; c.fillStyle = kind === 0 ? desktop.accent : desktop.foreground;
            c.lineWidth = 1.6; c.lineCap = "round"; c.lineJoin = "round";
            const x = width / 2 - 9, y = height / 2 - 9;
            if (kind === 0) {
                for (let row = 0; row < 2; ++row) for (let col = 0; col < 2; ++col) c.fillRect(x + col * 10, y + row * 10, 7, 7);
            } else if (kind === 1) {
                c.beginPath(); c.moveTo(x, y + 3); c.lineTo(x + 7, y + 3); c.lineTo(x + 10, y + 6); c.lineTo(x + 18, y + 6); c.lineTo(x + 18, y + 18); c.lineTo(x, y + 18); c.closePath(); c.stroke();
            } else if (kind === 2) {
                c.strokeRect(x, y + 2, 18, 15); c.beginPath(); c.moveTo(x + 4, y + 6); c.lineTo(x + 8, y + 9); c.lineTo(x + 4, y + 12); c.moveTo(x + 10, y + 12); c.lineTo(x + 14, y + 12); c.stroke();
            } else {
                for (let row = 0; row < 3; ++row) { c.beginPath(); c.moveTo(x, y + 3 + row * 6); c.lineTo(x + 18, y + 3 + row * 6); c.stroke(); c.beginPath(); c.arc(x + (row === 1 ? 12 : 6), y + 3 + row * 6, 2, 0, 2 * Math.PI); c.fill(); }
            }
        }
        Connections { target: desktop; function onForegroundChanged() { glyph.requestPaint(); } function onAccentChanged() { glyph.requestPaint(); } }
    }
    Column {
        anchors.top: parent.top; anchors.topMargin: 12
        width: desktop.railWidth; spacing: 8
        Repeater {
            model: ["view-grid", "system-file-manager", "system-run"]
            delegate: ToolButton {
                required property string modelData
                required property int index
                anchors.horizontalCenter: parent.horizontalCenter
                width: 32; height: 32
                Accessible.name: ["Launcher position", "Files position", "Terminal position"][index]
                ToolTip.visible: hovered; ToolTip.text: Accessible.name + " · layout preview"
                contentItem: RailGlyph { kind: index; implicitWidth: 20; implicitHeight: 20 }
                background: Rectangle { radius: 10; color: parent.hovered ? desktop.accent : "transparent"; Behavior on color { ColorAnimation { duration: 120 } } }
                onClicked: desktop.showControls = !desktop.showControls
            }
        }
    }
    Column {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 12
        width: desktop.railWidth; spacing: 16
        ToolButton {
            anchors.horizontalCenter: parent.horizontalCenter; width: 32; height: 32
            Accessible.name: "Toggle layout controls"
            contentItem: RailGlyph { kind: 3; implicitWidth: 20; implicitHeight: 20 }
            onClicked: desktop.showControls = !desktop.showControls
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDateTime(new Date(), "hh\nmm"); color: desktop.foreground; horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
        }
    }
}
