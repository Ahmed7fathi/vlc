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

FocusScope {
    id: root

    property var pagePrefix: []

    ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // ── Formatting helpers ───────────────────────────────────────────────

    function formatBytes(bytes)
    {
        if (bytes <= 0)
            return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var i = 0
        var val = bytes
        while (val >= 1024 && i < units.length - 1)
        {
            val /= 1024
            i++
        }
        return val.toFixed(i === 0 ? 0 : 1) + " " + units[i]
    }

    function formatSpeed(bytesPerSec)
    {
        if (bytesPerSec <= 0)
            return ""
        return formatBytes(bytesPerSec) + "/s"
    }

    function formatETA(seconds)
    {
        if (seconds <= 0)
            return ""
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = Math.floor(seconds % 60)
        if (h > 0)
            return qsTr("%1:%2:%3").arg(h).arg(m.toString().padStart(2, "0")).arg(s.toString().padStart(2, "0"))
        return qsTr("%1:%2").arg(m).arg(s.toString().padStart(2, "0"))
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

                        required property string taskId
                        required property string title
                        required property real progress
                        required property string stateName
                        required property bool isTerminal
                        required property double speed
                        required property double eta
                        required property double downloadedBytes
                        required property double totalBytes
                        required property string errorMessage
                        required property bool isActive

                        // Background highlight on hover
                        Rectangle {
                            anchors.fill: parent
                            radius: VLCStyle.dp(4, VLCStyle.scale)
                            color: mouseArea.containsMouse ? theme.bg.primary : "transparent"
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: VLCStyle.margin_xsmall
                            spacing: VLCStyle.margin_xsmall

                            // Thumbnail placeholder
                            Rectangle {
                                Layout.preferredWidth: VLCStyle.dp(48, VLCStyle.scale)
                                Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)
                                Layout.alignment: Qt.AlignVCenter
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

                            // Info + progress
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.leftMargin: VLCStyle.margin_xxsmall
                                Layout.alignment: Qt.AlignVCenter
                                spacing: VLCStyle.margin_xxxsmall

                                // Title row
                                Widgets.ListLabel {
                                    Layout.fillWidth: true
                                    text: title
                                    color: theme.fg.primary
                                    font.pixelSize: VLCStyle.fontSize_normal
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
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
                                        color: stateName === "Failed" ? "#F44336" : theme.accent
                                        implicitWidth: (parent ? parent.width : 200) * (listProgressBar.visualPosition)
                                        implicitHeight: parent ? parent.height : VLCStyle.dp(4, VLCStyle.scale)
                                    }
                                }

                                // Speed / ETA / size info
                                Widgets.CaptionLabel {
                                    Layout.fillWidth: true
                                    visible: isActive && !isTerminal && speed > 0
                                    text: {
                                        var parts = []
                                        if (speed > 0)
                                            parts.push(formatSpeed(speed))
                                        if (eta > 0)
                                            parts.push(formatETA(eta) + " " + qsTr("remaining"))
                                        if (downloadedBytes > 0)
                                            parts.push(formatBytes(downloadedBytes) + " / " + (totalBytes > 0 ? formatBytes(totalBytes) : "?"))
                                        return parts.join(" · ")
                                    }
                                    color: theme.fg.secondary
                                    font.pixelSize: VLCStyle.fontSize_xsmall
                                    elide: Text.ElideRight
                                }

                                // Error message
                                Widgets.CaptionLabel {
                                    Layout.fillWidth: true
                                    visible: stateName === "Failed" && errorMessage.length > 0
                                    text: errorMessage
                                    color: "#F44336"
                                    font.pixelSize: VLCStyle.fontSize_xsmall
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            // State badge
                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
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

                            // ── Simple action buttons ────────────────────

                            Row {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: VLCStyle.margin_xxxsmall
                                visible: mouseArea.containsMouse || !isTerminal

                                // Retry (failed only)
                                Widgets.IconLabel {
                                    visible: stateName === "Failed"
                                    text: VLCIcons.ic_fluent_arrow_sync_24_regular
                                    color: mouseAreaRetry.containsMouse ? theme.accent : theme.fg.primary
                                    font.pixelSize: VLCStyle.icon_small

                                    MouseArea {
                                        id: mouseAreaRetry
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: DownloaderController.retryTask(taskId)
                                    }
                                }

                                // Pause/Resume
                                Widgets.IconLabel {
                                    visible: stateName === "Downloading" || stateName === "Paused"
                                    text: stateName === "Paused" ? VLCIcons.play_filled : VLCIcons.pause_filled
                                    color: mouseAreaPause.containsMouse ? theme.accent : theme.fg.primary
                                    font.pixelSize: VLCStyle.icon_small

                                    MouseArea {
                                        id: mouseAreaPause
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            if (stateName === "Paused")
                                                DownloaderController.resumeTask(taskId)
                                            else
                                                DownloaderController.pauseTask(taskId)
                                        }
                                    }
                                }

                                // Cancel
                                Widgets.IconLabel {
                                    visible: !isTerminal && stateName !== "Failed"
                                    text: VLCIcons.stop
                                    color: mouseAreaCancel.containsMouse ? "#F44336" : theme.fg.primary
                                    font.pixelSize: VLCStyle.icon_small

                                    MouseArea {
                                        id: mouseAreaCancel
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: DownloaderController.cancelTask(taskId)
                                    }
                                }

                                // Clear (terminal states)
                                Widgets.IconLabel {
                                    visible: isTerminal
                                    text: VLCIcons.clear
                                    color: mouseAreaClear.containsMouse ? theme.accent : theme.fg.secondary
                                    font.pixelSize: VLCStyle.icon_small

                                    MouseArea {
                                        id: mouseAreaClear
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: DownloaderController.cancelTask(taskId)
                                    }
                                }
                            }
                        }
                    }
                }

                // Empty state
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
}
