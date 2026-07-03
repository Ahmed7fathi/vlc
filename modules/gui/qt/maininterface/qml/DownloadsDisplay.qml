/*****************************************************************************
 * DownloadsDisplay.qml
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import VLC.Downloader
import VLC.Style
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Util

/**
 * @brief Full-page download queue view with inline list and progress dialog.
 *
 * Self-contained page — no external QML type dependencies. DownloaderController
 * is registered as a singleton in the VLC.MainInterface module (see mainui.cpp).
 */
FocusScope {
    id: root

    property var pagePrefix: []

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg.primary

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_normal
            anchors.rightMargin: VLCStyle.margin_normal
            anchors.topMargin: VLCStyle.margin_normal
            spacing: VLCStyle.margin_xxsmall

            // ── Header ───────────────────────────────────────────────────

            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_xsmall

                Widgets.SubtitleLabel {
                    Layout.fillWidth: true
                    text: qsTr("Download Queue")
                    color: theme.fg.primary
                    font.bold: true
                }

                Widgets.CaptionLabel {
                    id: countLabel
                    text: {
                        var m = DownloaderController.taskModel
                        return m ? qsTr("%1 tasks").arg(m.count) : ""
                    }
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }

                Widgets.ButtonExt {
                    id: clearCompletedBtn
                    text: qsTr("Clear completed")
                    font.pixelSize: VLCStyle.fontSize_small
                    enabled: {
                        var m = DownloaderController.taskModel
                        if (!m) return false
                        for (var i = 0; i < m.count; ++i) {
                            var idx = m.index(i, 0)
                            if (m.data(idx, 0x10E)) // IsTerminalRole
                                return true
                        }
                        return false
                    }
                    onClicked: DownloaderController.removeCompletedTasks()
                }
            }

            // ── Task list ────────────────────────────────────────────────

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: theme.bg.secondary
                radius: VLCStyle.dp(4, VLCStyle.scale)

                ListView {
                    id: taskList
                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_xxxsmall
                    clip: true
                    spacing: VLCStyle.margin_xxxsmall

                    model: DownloaderController.taskModel

                    delegate: Item {
                        width: taskList.width
                        height: VLCStyle.dp(64, VLCStyle.scale)

                        required property int index
                        required property string taskId
                        required property string title
                        required property real progress
                        required property string stateName
                        required property bool isTerminal

                        Rectangle {
                            anchors.fill: parent
                            radius: VLCStyle.dp(4, VLCStyle.scale)
                            color: mouseArea.containsMouse ? theme.bg.primary : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: VLCStyle.duration_veryShort }
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                progressDialog.taskId = taskId
                                progressDialog.open()
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: VLCStyle.margin_xsmall
                            spacing: VLCStyle.margin_xsmall

                            // ── Thumbnail placeholder ─────────────────
                            Rectangle {
                                Layout.preferredWidth: VLCStyle.dp(48, VLCStyle.scale)
                                Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)
                                color: theme.border
                                radius: VLCStyle.dp(2, VLCStyle.scale)

                                Widgets.IconLabel {
                                    anchors.centerIn: parent
                                    text: VLCIcons.album_cover
                                    font.pixelSize: VLCStyle.icon_small
                                    color: theme.fg.secondary
                                    opacity: 0.5
                                }
                            }

                            // ── Info + progress ──────────────────────────
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: VLCStyle.margin_xxsmall
                                spacing: VLCStyle.margin_xxxsmall

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: VLCStyle.margin_xxsmall

                                    Widgets.ListLabel {
                                        Layout.fillWidth: true
                                        text: title
                                        color: theme.fg.primary
                                        font.pixelSize: VLCStyle.fontSize_normal
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }

                                    // State badge
                                    Rectangle {
                                        color: {
                                            if (stateName === "Completed") return "#4CAF50"
                                            if (stateName === "Failed") return "#F44336"
                                            if (stateName === "Downloading") return theme.accent
                                            if (stateName === "Paused") return "#FF9800"
                                            if (stateName === "Cancelled") return "#9E9E9E"
                                            return theme.border
                                        }
                                        opacity: 0.8
                                        radius: VLCStyle.dp(2, VLCStyle.scale)
                                        implicitWidth: badgeLabel.implicitWidth + VLCStyle.margin_xsmall
                                        implicitHeight: badgeLabel.implicitHeight + VLCStyle.margin_xxxsmall

                                        Widgets.CaptionLabel {
                                            id: badgeLabel
                                            anchors.centerIn: parent
                                            text: stateName
                                            color: "white"
                                            font.pixelSize: VLCStyle.fontSize_xsmall
                                            font.bold: true
                                        }
                                    }
                                }

                                // Progress bar
                                ProgressBar {
                                    id: listProgressBar
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: VLCStyle.dp(4, VLCStyle.scale)

                                    from: 0
                                    to: 100
                                    value: progress
                                    visible: !isTerminal

                                    background: Rectangle {
                                        color: theme.border
                                        radius: VLCStyle.dp(2, VLCStyle.scale)
                                    }

                                    contentItem: Rectangle {
                                        radius: VLCStyle.dp(2, VLCStyle.scale)
                                        color: theme.accent
                                        implicitWidth: (parent ? parent.width : 200) * (listProgressBar.visualPosition)
                                        implicitHeight: parent ? parent.height : VLCStyle.dp(4, VLCStyle.scale)
                                    }
                                }
                            }

                            // ── Action buttons ────────────────────────────
                            RowLayout {
                                spacing: VLCStyle.margin_xxsmall

                                Widgets.IconToolButton {
                                    text: {
                                        var m = DownloaderController.taskModel
                                        var idx = m.index(index, 0)
                                        var state = m.data(idx, 0x104) // StateRole
                                        return (state === 4) ? VLCIcons.play_filled : VLCIcons.pause
                                    }
                                    font.pixelSize: VLCStyle.icon_small
                                    visible: {
                                        var m = DownloaderController.taskModel
                                        var idx = m.index(index, 0)
                                        var state = m.data(idx, 0x104)
                                        return (state === 4 || state === 5)
                                    }
                                    Accessible.description: qsTr("Pause/Resume")

                                    onClicked: {
                                        var m = DownloaderController.taskModel
                                        var idx = m.index(index, 0)
                                        var state = m.data(idx, 0x104)
                                        if (state === 4)
                                            DownloaderController.resumeTask(taskId)
                                        else
                                            DownloaderController.pauseTask(taskId)
                                    }
                                }

                                Widgets.IconToolButton {
                                    text: VLCIcons.stop
                                    font.pixelSize: VLCStyle.icon_small
                                    visible: !isTerminal
                                    Accessible.description: qsTr("Cancel")
                                    onClicked: DownloaderController.cancelTask(taskId)
                                }

                                Widgets.IconToolButton {
                                    text: VLCIcons.reload
                                    font.pixelSize: VLCStyle.icon_small
                                    visible: stateName === "Failed"
                                    Accessible.description: qsTr("Retry")
                                    onClicked: DownloaderController.retryTask(taskId)
                                }
                            }
                        }
                    }
                }

                // ── Empty state ───────────────────────────────────────────
                Item {
                    anchors.centerIn: parent
                    visible: taskList.count === 0

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: VLCStyle.margin_small

                        Widgets.IconLabel {
                            Layout.alignment: Qt.AlignHCenter
                            text: VLCIcons.eject
                            font.pixelSize: VLCStyle.icon_xxlarge
                            color: theme.fg.secondary
                            opacity: 0.3
                        }

                        Widgets.CaptionLabel {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No downloads in queue")
                            color: theme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_large
                        }

                        Widgets.CaptionLabel {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Analyze a video URL to start downloading")
                            color: theme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_small
                        }
                    }
                }
            }
        }
    }

    // ── Progress dialog ─────────────────────────────────────────────────────

    Dialog {
        id: progressDialog

        property string taskId: ""

        title: qsTr("Download Progress")
        modal: false
        focus: true
        width: VLCStyle.dp(480, VLCStyle.scale)
        height: VLCStyle.dp(200, VLCStyle.scale)
        padding: 0
        closePolicy: Popup.CloseOnEscape

        readonly property ColorContext dialogTheme: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        function _taskRow() {
            var m = DownloaderController.taskModel
            if (!m || progressDialog.taskId === "")
                return -1
            for (var i = 0; i < m.count; ++i) {
                var idx = m.index(i, 0)
                if (m.data(idx, 0x101) === progressDialog.taskId)
                    return i
            }
            return -1
        }

        background: Rectangle {
            color: dialogTheme.bg.primary
            radius: VLCStyle.dp(10, VLCStyle.scale)
            border.color: Qt.rgba(dialogTheme.border.r, dialogTheme.border.g, dialogTheme.border.b, 0.15)
        }

        header: Rectangle {
            height: VLCStyle.dp(40, VLCStyle.scale)
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.margin_normal
                anchors.rightMargin: VLCStyle.margin_xsmall

                Widgets.IconLabel {
                    text: VLCIcons.eject
                    font.pixelSize: VLCStyle.icon_small
                    color: dialogTheme.accent
                }

                Widgets.CaptionLabel {
                    Layout.fillWidth: true
                    text: qsTr("Downloading")
                    color: dialogTheme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_large
                    font.bold: true
                    leftPadding: VLCStyle.margin_xsmall
                    verticalAlignment: Text.AlignVCenter
                }

                Widgets.IconToolButton {
                    text: VLCIcons.stop
                    font.pixelSize: VLCStyle.icon_small
                    opacity: hovered ? 1.0 : 0.6
                    Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }
                    onClicked: progressDialog.close()
                    Accessible.description: qsTr("Close")
                }
            }
        }

        contentItem: Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: VLCStyle.margin_normal
                anchors.topMargin: VLCStyle.margin_xsmall
                spacing: VLCStyle.margin_small

                Widgets.ListLabel {
                    Layout.fillWidth: true
                    text: {
                        var row = _taskRow()
                        if (row < 0) return progressDialog.taskId
                        return DownloaderController.taskModel.data(
                            DownloaderController.taskModel.index(row, 0), 0x103)
                    }
                    color: dialogTheme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: VLCStyle.margin_small

                    ProgressBar {
                        id: dlgProgressBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: VLCStyle.dp(6, VLCStyle.scale)
                        from: 0; to: 100
                        value: {
                            var row = _taskRow()
                            if (row < 0) return 0
                            return DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x106)
                        }
                        background: Rectangle {
                            color: dialogTheme.border; radius: VLCStyle.dp(3, VLCStyle.scale); opacity: 0.3
                        }
                        contentItem: Rectangle {
                            radius: VLCStyle.dp(3, VLCStyle.scale); color: dialogTheme.accent
                            Behavior on implicitWidth {
                                NumberAnimation { duration: VLCStyle.duration_short; easing.type: Easing.OutCubic }
                            }
                            implicitWidth: (parent ? parent.width : 200) * (dlgProgressBar.visualPosition)
                            implicitHeight: parent ? parent.height : VLCStyle.dp(6, VLCStyle.scale)
                        }
                    }

                    Widgets.IconLabel {
                        text: {
                            var row = _taskRow()
                            if (row < 0) return "0%"
                            var pct = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x106)
                            return "%1%".arg(pct)
                        }
                        color: dialogTheme.accent
                        font.pixelSize: VLCStyle.fontSize_large
                        font.bold: true
                        Layout.preferredWidth: VLCStyle.dp(48, VLCStyle.scale)
                        horizontalAlignment: Text.AlignRight
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: VLCStyle.margin_xxsmall
                    columns: 3
                    columnSpacing: VLCStyle.margin_large
                    rowSpacing: VLCStyle.margin_xxxsmall

                    Widgets.CaptionLabel {
                        text: qsTr("Speed"); color: dialogTheme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                    Widgets.CaptionLabel {
                        Layout.fillWidth: true
                        text: {
                            var row = _taskRow()
                            if (row < 0) return "--"
                            var speed = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x107)
                            return formatSpeed(speed)
                        }
                        color: dialogTheme.fg.primary; font.pixelSize: VLCStyle.fontSize_small; font.bold: true
                    }
                    Item { Layout.fillWidth: true }

                    Widgets.CaptionLabel {
                        text: qsTr("ETA"); color: dialogTheme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                    Widgets.CaptionLabel {
                        Layout.fillWidth: true
                        text: {
                            var row = _taskRow()
                            if (row < 0) return "--"
                            var eta = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x108)
                            return formatETA(eta)
                        }
                        color: dialogTheme.fg.primary; font.pixelSize: VLCStyle.fontSize_small; font.bold: true
                    }
                    Item { Layout.fillWidth: true }

                    Widgets.CaptionLabel {
                        text: qsTr("Size"); color: dialogTheme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                    Widgets.CaptionLabel {
                        Layout.fillWidth: true
                        text: {
                            var row = _taskRow()
                            if (row < 0) return "--"
                            var dl = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x109)
                            var total = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x10A)
                            return formatSize(dl, total)
                        }
                        color: dialogTheme.fg.primary; font.pixelSize: VLCStyle.fontSize_small; font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: VLCStyle.dp(1, VLCStyle.scale)
                    color: dialogTheme.border; opacity: 0.2
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: VLCStyle.margin_xsmall

                    Widgets.ButtonExt {
                        text: {
                            var row = _taskRow()
                            if (row < 0) return qsTr("Pause")
                            var state = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x104)
                            return (state === 4) ? qsTr("Resume") : qsTr("Pause")
                        }
                        enabled: {
                            var row = _taskRow()
                            if (row < 0) return false
                            var state = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x104)
                            return (state === 4 || state === 5)
                        }
                        onClicked: {
                            var row = _taskRow()
                            if (row < 0) return
                            var state = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x104)
                            if (state === 4)
                                DownloaderController.resumeTask(progressDialog.taskId)
                            else
                                DownloaderController.pauseTask(progressDialog.taskId)
                        }
                    }

                    Widgets.ButtonExt {
                        text: qsTr("Cancel")
                        enabled: {
                            var row = _taskRow()
                            if (row < 0) return false
                            var state = DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x104)
                            return (state !== 10)
                        }
                        onClicked: {
                            DownloaderController.cancelTask(progressDialog.taskId)
                            progressDialog.close()
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Widgets.CaptionLabel {
                        id: dialogStateLabel
                        text: {
                            var row = _taskRow()
                            if (row < 0) return ""
                            return DownloaderController.taskModel.data(
                                DownloaderController.taskModel.index(row, 0), 0x105)
                        }
                        color: dialogTheme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.implicitWidth + VLCStyle.margin_small
                            height: parent.implicitHeight + VLCStyle.margin_xxxsmall
                            radius: VLCStyle.dp(3, VLCStyle.scale)
                            color: {
                                var t = dialogStateLabel.text
                                if (t === "Downloading") return dialogTheme.accent
                                if (t === "Paused") return "#FF9800"
                                if (t === "Failed") return "#F44336"
                                if (t === "Completed") return "#4CAF50"
                                return "transparent"
                            }
                            opacity: 0.15; z: -1
                        }
                    }
                }
            }
        }
    }

    // ── Formatting helpers ────────────────────────────────────────────────

    function formatSpeed(bytesPerSec) {
        if (bytesPerSec <= 0) return "--"
        if (bytesPerSec < 1024) return qsTr("%1 B/s").arg(bytesPerSec.toFixed(0))
        if (bytesPerSec < 1048576) return qsTr("%1 KiB/s").arg((bytesPerSec / 1024).toFixed(1))
        if (bytesPerSec < 1073741824) return qsTr("%1 MiB/s").arg((bytesPerSec / 1048576).toFixed(1))
        return qsTr("%1 GiB/s").arg((bytesPerSec / 1073741824).toFixed(2))
    }

    function formatETA(seconds) {
        if (seconds <= 0) return "--"
        if (seconds < 60) return qsTr("%1s").arg(seconds)
        if (seconds < 3600) {
            var m = Math.floor(seconds / 60); var s = seconds % 60
            return qsTr("%1m %2s").arg(m).arg(s)
        }
        var h = Math.floor(seconds / 3600); var min = Math.floor((seconds % 3600) / 60)
        return qsTr("%1h %2m").arg(h).arg(min)
    }

    function formatSize(downloaded, total) {
        if (downloaded <= 0 && total <= 0) return "--"
        var fmtDl = formatBytes(downloaded)
        if (total <= 0) return fmtDl
        return qsTr("%1 / %2").arg(fmtDl).arg(formatBytes(total))
    }

    function formatBytes(bytes) {
        if (bytes <= 0) return "0 B"
        if (bytes < 1024) return qsTr("%1 B").arg(bytes.toFixed(0))
        if (bytes < 1048576) return qsTr("%1 KiB").arg((bytes / 1024).toFixed(1))
        if (bytes < 1073741824) return qsTr("%1 MiB").arg((bytes / 1048576).toFixed(1))
        return qsTr("%1 GiB").arg((bytes / 1073741824).toFixed(2))
    }
}
