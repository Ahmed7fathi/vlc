/*****************************************************************************
 * SubtitleSelector.qml
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

/**
 * @brief Subtitle track selector for choosing subtitle tracks.
 *
 * Displays available subtitle tracks with language and type
 * information. Supports multi-select (multiple subtitle tracks
 * can be selected). The user can toggle each track on/off.
 *
 * Usage:
 * @code
 *   SubtitleSelector {
 *       model: DownloaderController.taskModel
 *       taskRow: 0
 *       embedSubtitles: true
 *       onSubtitlesChanged: (indices) => { ... }
 *   }
 * @endcode
 */
FocusScope {
    id: root

    /** @brief The task model to get subtitle data from. */
    property var model: null

    /** @brief Row index of the task in the model. */
    property int taskRow: -1

    /** @brief Whether to embed subtitles into the output file. */
    property bool embedSubtitles: false

    /** @brief List of selected subtitle track indices. */
    property var selectedIndices: []

    /** @brief Emitted when subtitle selection changes. */
    signal subtitlesChanged(var indices)

    implicitHeight: layout.implicitHeight

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: VLCStyle.margin_xxsmall

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            text: qsTr("Subtitles")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        Widgets.CaptionLabel {
            Layout.fillWidth: true
            text: qsTr("Select subtitle tracks to download")
            color: root.colorContext.fg.secondary
            wrapMode: Text.WordWrap
        }

        ListView {
            id: subtitleList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: VLCStyle.margin_small

            clip: true
            spacing: VLCStyle.margin_xxsmall

            model: ListModel {
                id: subtitleModel
            }

            delegate: Item {
                width: subtitleList.width
                height: VLCStyle.dp(44, VLCStyle.scale)

                required property int modelIndex
                required property string language
                required property string type
                required property bool checked

                Rectangle {
                    anchors.fill: parent
                    radius: VLCStyle.dp(4, VLCStyle.scale)
                    color: checked ? theme.accent : "transparent"
                    opacity: checked ? 0.1 : 0.0

                    Behavior on opacity {
                        NumberAnimation { duration: VLCStyle.duration_short }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var newChecked = !checked
                        subtitleModel.setProperty(modelIndex, "checked", newChecked)

                        // Update selected indices
                        var indices = []
                        for (var i = 0; i < subtitleModel.count; ++i) {
                            if (subtitleModel.get(i).checked)
                                indices.push(i)
                        }
                        root.selectedIndices = indices
                        root.subtitlesChanged(indices)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_xsmall
                    spacing: VLCStyle.margin_xsmall

                    Widgets.CheckBoxExt {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: checked

                        onClicked: {
                            subtitleModel.setProperty(modelIndex, "checked", !checked)

                            var indices = []
                            for (var i = 0; i < subtitleModel.count; ++i) {
                                if (subtitleModel.get(i).checked)
                                    indices.push(i)
                            }
                            root.selectedIndices = indices
                            root.subtitlesChanged(indices)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        spacing: VLCStyle.margin_xxxsmall

                        Widgets.IconLabel {
                            Layout.fillWidth: true
                            text: language
                            color: root.colorContext.fg.primary
                            font.pixelSize: VLCStyle.fontSize_normal
                        }

                        Widgets.CaptionLabel {
                            Layout.fillWidth: true
                            text: type
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_small
                        }
                    }

                    Widgets.CaptionLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Download")
                        color: root.colorContext.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: VLCStyle.dp(1, VLCStyle.scale)
                    color: theme.border
                    opacity: 0.3
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_xsmall

            Widgets.CheckBoxExt {
                id: embedCheckbox
                text: qsTr("Embed subtitles in output file")
                checked: root.embedSubtitles
                onClicked: root.embedSubtitles = checked
            }

            Item { Layout.fillWidth: true }
        }
    }

    /**
     * @brief Populate the subtitle list from real MediaInfo data.
     * @param taskId  The task ID to load subtitles for.
     */
    function loadFromTask(taskId) {
        subtitleModel.clear()

        if (taskId === "")
            return

        // Use the language name for display; fall back to language code
        var subtitles = DownloaderController.subtitleFormatsForTask(taskId)
        if (!subtitles || subtitles.length === 0)
            return

        for (var i = 0; i < subtitles.length; ++i) {
            var s = subtitles[i]
            var displayLang = s.name || s.language || qsTr("Unknown")
            subtitleModel.append({
                modelIndex: i,
                language: displayLang,
                type: s.type || (s.isAutomatic ? qsTr("Automatic (VTT)") : qsTr("External (SRT)")),
                checked: false
            })
        }

        // Auto-select first subtitle
        if (subtitleModel.count > 0) {
            subtitleModel.setProperty(0, "checked", true)
            root.selectedIndices = [0]
            root.subtitlesChanged([0])
        }
    }
}
