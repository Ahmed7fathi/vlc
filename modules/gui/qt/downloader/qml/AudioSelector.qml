/*****************************************************************************
 * AudioSelector.qml
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
 * @brief Audio format selector for choosing an audio track/format.
 *
 * Displays available audio formats with codec, bitrate, language,
 * and track information. The user selects one format to download.
 * Only visible when audioOnly mode is enabled.
 *
 * Usage:
 * @code
 *   AudioSelector {
 *       audioOnly: true
 *       onFormatSelected: (formatIndex) => { ... }
 *   }
 *
 *   // After the task is analyzed:
 *   audioSelector.loadFromTask(taskId)
 * @endcode
 */
FocusScope {
    id: root

    /** @brief Whether audio-only mode is enabled (controls visibility). */
    property bool audioOnly: false

    /** @brief Currently selected format index. */
    property int selectedIndex: -1

    /** @brief Emitted when a format is selected. */
    signal formatSelected(int formatIndex)

    implicitHeight: visible ? layout.implicitHeight : 0
    visible: root.audioOnly

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
            text: qsTr("Audio Format")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        Widgets.CaptionLabel {
            Layout.fillWidth: true
            text: qsTr("Select the audio quality and format")
            color: root.colorContext.fg.secondary
            wrapMode: Text.WordWrap
        }

        ListView {
            id: formatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: VLCStyle.margin_small

            clip: true
            spacing: VLCStyle.margin_xxsmall

            model: ListModel {
                id: formatModel
            }

            delegate: Item {
                width: formatList.width
                height: VLCStyle.dp(48, VLCStyle.scale)

                required property int modelIndex
                required property string name
                required property string codec
                required property string bitrate
                required property string sampleRate
                required property bool selected

                Rectangle {
                    anchors.fill: parent
                    radius: VLCStyle.dp(4, VLCStyle.scale)
                    color: selected ? theme.accent : "transparent"
                    opacity: selected ? 0.15 : 0.0

                    Behavior on opacity {
                        NumberAnimation { duration: VLCStyle.duration_short }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: selectThisFormat(modelIndex)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_xsmall
                    spacing: VLCStyle.margin_xsmall

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: VLCStyle.margin_xxxsmall

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: VLCStyle.margin_xxsmall

                            Widgets.IconLabel {
                                text: name
                                color: root.colorContext.fg.primary
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.bold: true
                            }

                            Rectangle {
                                visible: codec !== ""
                                color: theme.accent
                                opacity: 0.3
                                radius: VLCStyle.dp(2, VLCStyle.scale)
                                implicitWidth: codecLabel.implicitWidth + VLCStyle.margin_xsmall
                                implicitHeight: codecLabel.implicitHeight + VLCStyle.margin_xxxsmall

                                Widgets.CaptionLabel {
                                    id: codecLabel
                                    anchors.centerIn: parent
                                    text: codec
                                    color: root.colorContext.fg.primary
                                    font.pixelSize: VLCStyle.fontSize_xsmall
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: VLCStyle.margin_normal

                            Widgets.CaptionLabel {
                                text: bitrate
                                color: root.colorContext.fg.secondary
                                font.pixelSize: VLCStyle.fontSize_small
                            }

                            Widgets.CaptionLabel {
                                text: sampleRate
                                color: root.colorContext.fg.secondary
                                font.pixelSize: VLCStyle.fontSize_small
                            }
                        }
                    }

                    Widgets.RadioButtonExt {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: selected
                        onClicked: selectThisFormat(modelIndex)
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
    }

    /**
     * @brief Select a format and emit the signal.
     */
    function selectThisFormat(index) {
        if (root.selectedIndex === index)
            return

        for (var i = 0; i < formatModel.count; ++i)
            formatModel.setProperty(i, "selected", (i === index))

        root.selectedIndex = index
        root.formatSelected(index)
    }

    /**
     * @brief Populate the audio format list from real MediaInfo data.
     * @param taskId  The task ID to load formats for.
     */
    function loadFromTask(taskId) {
        formatModel.clear()

        if (taskId === "")
            return

        var formats = DownloaderController.audioFormatsForTask(taskId)
        if (!formats || formats.length === 0)
            return

        for (var i = 0; i < formats.length; ++i) {
            var f = formats[i]
            formatModel.append({
                modelIndex: i,
                name: f.name,
                codec: f.codec,
                bitrate: f.bitrate || "",
                sampleRate: f.sampleRate || "",
                selected: false
            })
        }

        // Auto-select best audio
        if (formats.length > 0)
            selectThisFormat(0)
    }
}
