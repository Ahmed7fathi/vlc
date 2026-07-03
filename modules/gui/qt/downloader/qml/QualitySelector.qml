/*****************************************************************************
 * QualitySelector.qml
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
 * @brief Video quality selector for choosing a video format.
 *
 * Displays available video formats in a list with resolution,
 * codec, bitrate, and file size information. The user selects
 * one format to download via radio button or click.
 *
 * Usage:
 * @code
 *   QualitySelector {
 *       id: qualitySelector
 *       onFormatSelected: (formatIndex) => { ... }
 *   }
 *
 *   // After the task is analyzed:
 *   qualitySelector.loadFromTask(taskId)
 * @endcode
 */
FocusScope {
    id: root

    /** @brief Currently selected format index. */
    property int selectedIndex: -1

    /** @brief Emitted when a format is selected. */
    signal formatSelected(int formatIndex)

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
            text: qsTr("Video Quality")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        Widgets.CaptionLabel {
            Layout.fillWidth: true
            text: qsTr("Select the video resolution and format to download")
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
                height: VLCStyle.dp(56, VLCStyle.scale)

                required property int modelIndex
                required property string resolution
                required property string codec
                required property string bitrate
                required property string fileSize
                required property string extension
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
                                text: resolution
                                color: root.colorContext.fg.primary
                                font.pixelSize: VLCStyle.fontSize_large
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

                            Rectangle {
                                visible: extension !== ""
                                color: theme.border
                                radius: VLCStyle.dp(2, VLCStyle.scale)
                                implicitWidth: extLabel.implicitWidth + VLCStyle.margin_xsmall
                                implicitHeight: extLabel.implicitHeight + VLCStyle.margin_xxxsmall

                                Widgets.CaptionLabel {
                                    id: extLabel
                                    anchors.centerIn: parent
                                    text: extension.toUpperCase()
                                    color: root.colorContext.fg.secondary
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
                                text: fileSize
                                color: root.colorContext.fg.secondary
                                font.pixelSize: VLCStyle.fontSize_small
                            }
                        }
                    }

                    Widgets.RadioButtonExt {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: selected
                        // Only the MouseArea emits selection signals to avoid double emission
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

        // Update model selection state
        for (var i = 0; i < formatModel.count; ++i)
            formatModel.setProperty(i, "selected", (i === index))

        root.selectedIndex = index
        root.formatSelected(index)
    }

    /**
     * @brief Populate the format list from real MediaInfo data.
     * @param taskId  The task ID to load formats for.
     */
    function loadFromTask(taskId) {
        formatModel.clear()

        if (taskId === "")
            return

        var formats = DownloaderController.videoFormatsForTask(taskId)
        if (!formats || formats.length === 0)
            return

        for (var i = 0; i < formats.length; ++i) {
            var f = formats[i]
            formatModel.append({
                modelIndex: i,
                resolution: f.resolution,
                codec: f.codec,
                bitrate: f.bitrate || "",
                fileSize: f.fileSize || "",
                extension: f.extension,
                selected: false
            })
        }

        // Auto-select the best quality (first entry is typically the highest)
        selectThisFormat(0)
    }
}
