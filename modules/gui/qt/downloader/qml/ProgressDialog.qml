/*****************************************************************************
 * ProgressDialog.qml
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
import QtQuick.Templates as T

import VLC.Style
import VLC.Widgets as Widgets
import VLC.MainInterface

/**
 * @brief Real-time download progress dialog for a single task.
 *
 * Displays:
 *   - Task title and URL
 *   - Animated progress bar with percentage
 *   - Download speed, ETA, bytes downloaded/total
 *   - Current file being processed
 *   - Pause/Resume and Cancel controls
 *
 * Usage:
 * @code
 *   ProgressDialog {
 *       taskId: "some-task-id"
 *   }
 * @endcode
 *
 * The dialog updates automatically via the DownloaderController's
 * task model (DownloadTaskModel) which receives EventBus updates.
 */
Dialog {
    id: root

    property string taskId: ""

    title: qsTr("Download Progress")

    modal: false
    focus: true

    width: VLCStyle.dp(480, VLCStyle.scale)
    height: VLCStyle.dp(200, VLCStyle.scale)

    padding: 0

    closePolicy: Popup.CloseOnEscape

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    // ── Lookup task row from model ───────────────────────────────────────

    function _taskRow() {
        var model = DownloaderController.taskModel
        if (!model || root.taskId === "")
            return -1
        for (var i = 0; i < model.count; ++i) {
            var idx = model.index(i, 0)
            if (model.data(idx, 0x101) === root.taskId) // TaskIdRole = Qt.UserRole + 1 = 0x101
                return i
        }
        return -1
    }

    // ── Background ────────────────────────────────────────────────────────

    background: Rectangle {
        color: theme.bg.primary
        radius: VLCStyle.dp(10, VLCStyle.scale)
        border.color: Qt.rgba(theme.border.r, theme.border.g, theme.border.b, 0.15)

        // Accent top bar
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: VLCStyle.dp(3, VLCStyle.scale)
            color: theme.accent
            radius: VLCStyle.dp(10, VLCStyle.scale)

            // Only round top corners
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.radius
                color: theme.bg.primary
            }
        }
    }

    // ── Header ────────────────────────────────────────────────────────────

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
                color: theme.accent
            }

            Widgets.CaptionLabel {
                Layout.fillWidth: true
                text: qsTr("Downloading")
                color: theme.fg.primary
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
                onClicked: root.close()
                Accessible.description: qsTr("Close")
            }
        }
    }

    // ── Content ───────────────────────────────────────────────────────────

    contentItem: Item {
        id: contentArea

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal
            anchors.topMargin: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_small

            // Title
            Widgets.ListLabel {
                id: titleLabel
                Layout.fillWidth: true
                text: {
                    var row = _taskRow()
                    if (row < 0) return root.taskId
                    var idx = DownloaderController.taskModel.index(row, 0)
                    return DownloaderController.taskModel.data(idx, 0x103) // TitleRole = Qt.UserRole + 3
                }
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // Progress bar with label
            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_small

                ProgressBar {
                    id: progressBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.dp(6, VLCStyle.scale)

                    from: 0
                    to: 100

                    value: {
                        var row = _taskRow()
                        if (row < 0) return 0
                        var idx = DownloaderController.taskModel.index(row, 0)
                        return DownloaderController.taskModel.data(idx, 0x106) // ProgressRole = Qt.UserRole + 6
                    }

                    background: Rectangle {
                        color: theme.border
                        radius: VLCStyle.dp(3, VLCStyle.scale)
                        opacity: 0.3
                    }

                    contentItem: Rectangle {
                        radius: VLCStyle.dp(3, VLCStyle.scale)
                        color: theme.accent

                        Behavior on implicitWidth {
                            NumberAnimation { duration: VLCStyle.duration_short; easing.type: Easing.OutCubic }
                        }

                        implicitWidth: (parent ? parent.width : 200) * (progressBar.visualPosition)
                        implicitHeight: parent ? parent.height : VLCStyle.dp(6, VLCStyle.scale)
                    }
                }

                Widgets.IconLabel {
                    id: percentLabel
                    text: {
                        var row = _taskRow()
                        if (row < 0) return "0%"
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var pct = DownloaderController.taskModel.data(idx, 0x106) // ProgressRole = Qt.UserRole + 6
                        return "%1%".arg(pct)
                    }
                    color: theme.accent
                    font.pixelSize: VLCStyle.fontSize_large
                    font.bold: true
                    Layout.preferredWidth: VLCStyle.dp(48, VLCStyle.scale)
                    horizontalAlignment: Text.AlignRight
                }
            }

            // Stats grid
            GridLayout {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_xxsmall
                columns: 3
                columnSpacing: VLCStyle.margin_large
                rowSpacing: VLCStyle.margin_xxxsmall

                // Speed
                Widgets.CaptionLabel {
                    text: qsTr("Speed")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }

                Widgets.CaptionLabel {
                    id: speedLabel
                    Layout.fillWidth: true
                    text: {
                        var row = _taskRow()
                        if (row < 0) return "--"
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var speed = DownloaderController.taskModel.data(idx, 0x107) // SpeedRole = Qt.UserRole + 7
                        return formatSpeed(speed)
                    }
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_small
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                // ETA
                Widgets.CaptionLabel {
                    text: qsTr("ETA")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }

                Widgets.CaptionLabel {
                    id: etaLabel
                    Layout.fillWidth: true
                    text: {
                        var row = _taskRow()
                        if (row < 0) return "--"
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var eta = DownloaderController.taskModel.data(idx, 0x108) // EtaRole = Qt.UserRole + 8
                        return formatETA(eta)
                    }
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_small
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                // Size
                Widgets.CaptionLabel {
                    text: qsTr("Size")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }

                Widgets.CaptionLabel {
                    id: sizeLabel
                    Layout.fillWidth: true
                    text: {
                        var row = _taskRow()
                        if (row < 0) return "--"
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var downloaded = DownloaderController.taskModel.data(idx, 0x109) // DownloadedBytesRole = Qt.UserRole + 9
                        var total = DownloaderController.taskModel.data(idx, 0x10A) // TotalBytesRole = Qt.UserRole + 10
                        return formatSize(downloaded, total)
                    }
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_small
                    font.bold: true
                }

                Item { Layout.fillWidth: true }
            }

            // ── Separator ───────────────────────────────────────────────

            Rectangle {
                Layout.fillWidth: true
                height: VLCStyle.dp(1, VLCStyle.scale)
                color: theme.border
                opacity: 0.2
            }

            // ── Control buttons ──────────────────────────────────────────

            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_xsmall

                Widgets.ButtonExt {
                    id: pauseResumeBtn
                    text: {
                        var row = _taskRow()
                        if (row < 0) return qsTr("Pause")
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var state = DownloaderController.taskModel.data(idx, 0x104) // StateRole
                        // State::Paused = 4, State::Downloading = 5
                        return (state === 4) ? qsTr("Resume") : qsTr("Pause")
                    }
                    enabled: {
                        var row = _taskRow()
                        if (row < 0) return false
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var state = DownloaderController.taskModel.data(idx, 0x104)
                        // Only show when Paused (4) or Downloading (5)
                        return (state === 4 || state === 5)
                    }

                    onClicked: {
                        var row = _taskRow()
                        if (row < 0) return
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var state = DownloaderController.taskModel.data(idx, 0x104)
                        if (state === 4) // Paused
                            DownloaderController.resumeTask(root.taskId)
                        else
                            DownloaderController.pauseTask(root.taskId)
                    }
                }

                Widgets.ButtonExt {
                    text: qsTr("Cancel")
                    enabled: {
                        var row = _taskRow()
                        if (row < 0) return false
                        var idx = DownloaderController.taskModel.index(row, 0)
                        var state = DownloaderController.taskModel.data(idx, 0x104)
                        // Can cancel from any non-terminal state except Cancelled
                        return (state !== 10) // Cancelled
                    }

                    onClicked: {
                        DownloaderController.cancelTask(root.taskId)
                        root.close()
                    }
                }

                Item { Layout.fillWidth: true }

                Widgets.CaptionLabel {
                    id: stateLabel
                    text: {
                        var row = _taskRow()
                        if (row < 0) return ""
                        var idx = DownloaderController.taskModel.index(row, 0)
                        return DownloaderController.taskModel.data(idx, 0x105) // StateNameRole = Qt.UserRole + 5
                    }
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.implicitWidth + VLCStyle.margin_small
                        height: parent.implicitHeight + VLCStyle.margin_xxxsmall
                        radius: VLCStyle.dp(3, VLCStyle.scale)
                        color: {
                            if (stateLabel.text === "Downloading") return theme.accent
                            if (stateLabel.text === "Paused") return "#FF9800"
                            if (stateLabel.text === "Failed") return "#F44336"
                            if (stateLabel.text === "Completed") return "#4CAF50"
                            return "transparent"
                        }
                        opacity: 0.15
                        z: -1
                    }
                }
            }
        }
    }

    // ── Formatting helpers ────────────────────────────────────────────────

    function formatSpeed(bytesPerSec) {
        if (bytesPerSec <= 0) return "--"
        if (bytesPerSec < 1024)
            return qsTr("%1 B/s").arg(bytesPerSec.toFixed(0))
        if (bytesPerSec < 1024 * 1024)
            return qsTr("%1 KiB/s").arg((bytesPerSec / 1024).toFixed(1))
        if (bytesPerSec < 1024 * 1024 * 1024)
            return qsTr("%1 MiB/s").arg((bytesPerSec / (1024 * 1024)).toFixed(1))
        return qsTr("%1 GiB/s").arg((bytesPerSec / (1024 * 1024 * 1024)).toFixed(2))
    }

    function formatETA(seconds) {
        if (seconds <= 0) return "--"
        if (seconds < 60)
            return qsTr("%1s").arg(seconds)
        if (seconds < 3600) {
            var m = Math.floor(seconds / 60)
            var s = seconds % 60
            return qsTr("%1m %2s").arg(m).arg(s)
        }
        var h = Math.floor(seconds / 3600)
        var min = Math.floor((seconds % 3600) / 60)
        return qsTr("%1h %2m").arg(h).arg(min)
    }

    function formatSize(downloaded, total) {
        if (downloaded <= 0 && total <= 0) return "--"
        var fmtDownloaded = formatBytes(downloaded)
        if (total <= 0)
            return fmtDownloaded
        var fmtTotal = formatBytes(total)
        return qsTr("%1 / %2").arg(fmtDownloaded).arg(fmtTotal)
    }

    function formatBytes(bytes) {
        if (bytes <= 0) return "0 B"
        if (bytes < 1024)
            return qsTr("%1 B").arg(bytes.toFixed(0))
        if (bytes < 1024 * 1024)
            return qsTr("%1 KiB").arg((bytes / 1024).toFixed(1))
        if (bytes < 1024 * 1024 * 1024)
            return qsTr("%1 MiB").arg((bytes / (1024 * 1024)).toFixed(1))
        return qsTr("%1 GiB").arg((bytes / (1024 * 1024 * 1024)).toFixed(2))
    }
}
