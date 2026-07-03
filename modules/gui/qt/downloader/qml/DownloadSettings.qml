/*****************************************************************************
 * DownloadSettings.qml
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
import VLC.Downloader
import VLC.Util

/**
 * @brief Download settings panel for configuring download preferences.
 *
 * Allows the user to configure:
 *   - Default download directory
 *   - Preferred video quality (resolution)
 *   - Maximum concurrent downloads
 *   - Default filename template
 *   - Embed options (metadata, subtitles, chapters)
 *
 * Usage:
 * @code
 *   DownloadSettings {
 *       anchors.fill: parent
 *   }
 * @endcode
 */
FocusScope {
    id: root

    /** @brief Emitted when settings are saved. */
    signal saved()

    /** @brief Emitted when settings are reset to defaults. */
    signal resetToDefaults()

    implicitHeight: layout.implicitHeight

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        // ── Download Directory ───────────────────────────────────────────

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            text: qsTr("Download Location")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: pathInputLayout.implicitHeight + VLCStyle.margin_small * 2
            color: theme.bg.secondary
            radius: VLCStyle.dp(4, VLCStyle.scale)

            RowLayout {
                id: pathInputLayout
                anchors.fill: parent
                anchors.margins: VLCStyle.margin_small
                spacing: VLCStyle.margin_xsmall

                Widgets.TextFieldExt {
                    id: downloadPathField
                    Layout.fillWidth: true
                    text: DownloaderController.defaultDownloadPath
                    placeholderText: qsTr("/home/user/Downloads")
                    font.pixelSize: VLCStyle.fontSize_normal

                    onEditingFinished: {
                        DownloaderController.defaultDownloadPath = text
                    }
                }

                Widgets.ButtonExt {
                    text: qsTr("Browse...")
                    font.pixelSize: VLCStyle.fontSize_small
                    // TODO: Open native directory picker dialog
                }
            }
        }

        Widgets.CaptionLabel {
            Layout.fillWidth: true
            text: qsTr("Files will be saved to this directory")
            color: root.colorContext.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
            leftPadding: VLCStyle.margin_xxsmall
        }

        // ── Separator ────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height: VLCStyle.dp(1, VLCStyle.scale)
            color: theme.border
            opacity: 0.3
        }

        // ── Video Quality ────────────────────────────────────────────────

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            text: qsTr("Video Quality")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_xsmall

            Widgets.CaptionLabel {
                text: qsTr("Preferred resolution:")
                color: root.colorContext.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.ComboBoxExt {
                id: qualityCombo
                Layout.preferredWidth: VLCStyle.dp(160, VLCStyle.scale)

                model: ListModel {
                    ListElement { text: "2160p (4K)"; value: 2160 }
                    ListElement { text: "1440p (2K)"; value: 1440 }
                    ListElement { text: "1080p"; value: 1080 }
                    ListElement { text: "720p"; value: 720 }
                    ListElement { text: "480p"; value: 480 }
                    ListElement { text: "360p"; value: 360 }
                }

                currentIndex: {
                    // Find default quality index
                    var def = 720
                    for (var i = 0; i < model.count; ++i) {
                        if (model.get(i).value === def)
                            return i
                    }
                    return 2 // 1080p default
                }
            }
        }

        // ── Separator ────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height: VLCStyle.dp(1, VLCStyle.scale)
            color: theme.border
            opacity: 0.3
        }

        // ── Concurrent Downloads ─────────────────────────────────────────

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            text: qsTr("Concurrent Downloads")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_xsmall

            Widgets.CaptionLabel {
                text: qsTr("Maximum simultaneous downloads:")
                color: root.colorContext.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.SpinBoxExt {
                id: concurrentSpinBox
                Layout.preferredWidth: VLCStyle.dp(80, VLCStyle.scale)

                from: 1
                to: 10
                value: 2
                stepSize: 1

                textFromValue: function(value, locale) {
                    return Number(value).toLocaleString(locale, 'f', 0)
                }
                valueFromText: function(text, locale) {
                    return Number.fromLocaleString(locale, text)
                }
            }

            Widgets.CaptionLabel {
                text: qsTr("Higher values may use more bandwidth")
                color: root.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
            }
        }

        // ── Separator ────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height: VLCStyle.dp(1, VLCStyle.scale)
            color: theme.border
            opacity: 0.3
        }

        // ── Embed Options ────────────────────────────────────────────────

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            text: qsTr("Embedding")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_xxsmall

            Widgets.CheckBoxExt {
                id: embedMetadataChk
                text: qsTr("Embed metadata (title, artist, description, thumbnail)")
                checked: true
            }

            Widgets.CheckBoxExt {
                id: embedSubtitlesChk
                text: qsTr("Embed subtitles into the output file")
                checked: false
            }

            Widgets.CheckBoxExt {
                id: embedChaptersChk
                text: qsTr("Embed chapter markers")
                checked: true
            }
        }

        // ── Separator ────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height: VLCStyle.dp(1, VLCStyle.scale)
            color: theme.border
            opacity: 0.3
        }

        // ── Filename Template ────────────────────────────────────────────

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            text: qsTr("Filename Template")
            color: root.colorContext.fg.primary
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: templateLayout.implicitHeight + VLCStyle.margin_small * 2
            color: theme.bg.secondary
            radius: VLCStyle.dp(4, VLCStyle.scale)

            RowLayout {
                id: templateLayout
                anchors.fill: parent
                anchors.margins: VLCStyle.margin_small
                spacing: VLCStyle.margin_xsmall

                Widgets.TextFieldExt {
                    id: templateField
                    Layout.fillWidth: true
                    text: "%(title)s.%(ext)s"
                    placeholderText: "%(title)s.%(ext)s"
                    font.pixelSize: VLCStyle.fontSize_normal
                }

                Widgets.IconToolButton {
                    text: VLCIcons.info
                    font.pixelSize: VLCStyle.icon_small
                    Accessible.description: qsTr("Template variables info")
                    // TODO: Show tooltip with available variables
                }
            }
        }

        Widgets.CaptionLabel {
            Layout.fillWidth: true
            text: qsTr("Available variables: %(title)s, %(ext)s, %(id)s, %(uploader)s, %(duration)s")
            color: root.colorContext.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
            wrapMode: Text.WordWrap
        }

        // ── Footer Buttons ───────────────────────────────────────────────

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_normal
            spacing: VLCStyle.margin_xsmall

            Widgets.ButtonExt {
                text: qsTr("Reset to Defaults")
                onClicked: root.resetToDefaults()
            }

            Item { Layout.fillWidth: true }

            Widgets.ButtonExt {
                text: qsTr("Save")
                font.bold: true
                highlighted: true

                onClicked: {
                    DownloaderController.saveSettings()
                    root.saved()
                }
            }
        }
    }
}
