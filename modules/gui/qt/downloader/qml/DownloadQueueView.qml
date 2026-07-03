/*****************************************************************************
 * DownloadQueueView.qml
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

import VLC.Style
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Util

/**
 * @brief Download queue list view showing all tasks with their states.
 *
 * Displays a list of all download tasks with:
 *   - Title/URL, progress bar, percentage, speed, ETA
 *   - State badge (Downloading, Paused, Completed, Failed, etc.)
 *   - Action buttons: Cancel, Pause/Resume, Retry, Clear completed
 *
 * Usage:
 * @code
 *   DownloadQueueView {
 *       anchors.fill: parent
 *   }
 * @endcode
 */
FocusScope {
    id: root

    /** @brief Maximum number of visible items (0 = all). */
    property int maxVisible: 10

    /** @brief Emitted when a task's progress dialog should be shown. */
    signal showProgress(string taskId)

    implicitHeight: layout.implicitHeight

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Component.onCompleted: {
        // DownloaderController is available as a QML context property
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: VLCStyle.margin_xxsmall

        // ── Header ───────────────────────────────────────────────────────

        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_xsmall

            Widgets.SubtitleLabel {
                Layout.fillWidth: true
                text: qsTr("Download Queue")
                color: root.colorContext.fg.primary
                font.bold: true
            }

            Widgets.CaptionLabel {
                id: countLabel
                text: {
                    var model = DownloaderController.taskModel
                    if (!model) return ""
                    return qsTr("%1 tasks").arg(model.count)
                }
                color: root.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
            }

            Widgets.ButtonExt {
                id: clearCompletedBtn
                text: qsTr("Clear completed")
                font.pixelSize: VLCStyle.fontSize_small
                enabled: {
                    var model = DownloaderController.taskModel
                    if (!model) return false
                    for (var i = 0; i < model.count; ++i) {
                        var idx = model.index(i, 0)
                        if (model.data(idx, 0x10E)) // IsTerminalRole = Qt.UserRole + 14
                            return true
                    }
                    return false
                }

                onClicked: {
                    // Remove completed tasks from the model
                    DownloaderController.removeCompletedTasks()
                }
            }
        }

        // ── Task list ────────────────────────────────────────────────────

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

                    Component.onCompleted: {
                        console.log("[DownloadQueueView] delegate created: index="
                                    + taskList.model.data(taskList.model.index(index, 0), 0x101)
                                    + ", title=" + title
                                    + ", progress=" + progress
                                    + ", stateName=" + stateName
                                    + ", isTerminal=" + isTerminal)
                    }

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
                        onClicked: root.showProgress(taskId)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: VLCStyle.margin_xsmall
                        spacing: VLCStyle.margin_xsmall

                        // ── Left: thumbnail placeholder ─────────────────
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

                        // ── Center: info + progress ─────────────────────
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
                                    implicitWidth: (parent ? parent.width : 200) * (progressBar.visualPosition)
                                    implicitHeight: parent ? parent.height : VLCStyle.dp(4, VLCStyle.scale)
                                }
                            }
                        }

                        // ── Right: action buttons ───────────────────────
                        RowLayout {
                            spacing: VLCStyle.margin_xxsmall

                            Widgets.IconToolButton {
                                text: {
                                    // State check: Paused (4) -> play icon, else pause icon
                                    var idx = DownloaderController.taskModel.index(index, 0)
                                    var state = DownloaderController.taskModel.data(idx, 0x104)
                                    return (state === 4) ? VLCIcons.play_filled : VLCIcons.pause
                                }
                                font.pixelSize: VLCStyle.icon_small
                                visible: {
                                    var idx = DownloaderController.taskModel.index(index, 0)
                                    var state = DownloaderController.taskModel.data(idx, 0x104)
                                    return (state === 4 || state === 5) // Paused or Downloading
                                }
                                Accessible.description: qsTr("Pause/Resume")

                                onClicked: {
                                    var idx = DownloaderController.taskModel.index(index, 0)
                                    var state = DownloaderController.taskModel.data(idx, 0x104)
                                    if (state === 4) // Paused
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

            // ── Empty state ──────────────────────────────────────────────
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
