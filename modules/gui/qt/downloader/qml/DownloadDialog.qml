/*****************************************************************************
 * DownloadDialog.qml
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
 * @brief Main download dialog for analyzing and configuring media downloads.
 *
 * This dialog guides the user through the download process:
 *   1. Enter/paste a URL → analyze
 *   2. View media info (title, thumbnail, duration, description)
 *   3. Select video quality, audio format, and subtitle tracks
 *   4. Choose output options (embed metadata, subtitles)
 *   5. Start the download
 *
 * This is used as the root item in a QQuickWidget embedded in a QDialog.
 * We use Rectangle (not Dialog) as root because Dialog/Popup root items
 * have visibility issues when loaded inside a QQuickWidget.
 */
Rectangle {
    id: root

    color: theme.bg.primary

    /**
     * @brief Emitted when the user wants to close the dialog.
     * The C++ wrapper (DownloadDialog) connects this to QDialog::close().
     */
    signal requestClose()

    // ── Key handling ─────────────────────────────────────────────────────

    Keys.onEscapePressed: requestClose()
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape)
            requestClose()
    }
    focus: true

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    // ── Internal state ───────────────────────────────────────────────────

    /** @brief Optional initial URL set by the C++ wrapper via context property. */
    property string _initialUrl: typeof _initialDownloadUrl !== 'undefined' ? _initialDownloadUrl : ""

    property string _currentTaskId: ""
    property bool _isAnalyzing: false
    property bool _analysisComplete: false
    property string _videoTitle: ""
    property string _videoDuration: ""
    property string _videoUploader: ""
    property string _videoDescription: ""
    property string _thumbnailUrl: ""
    property int _selectedFormatIdx: -1
    property bool _audioOnly: false
    property var _selectedSubs: []
    property bool _formatsLoaded: false

    // ── Accent bar at top ────────────────────────────────────────────────

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: VLCStyle.dp(3, VLCStyle.scale)
        color: theme.accent
    }

    // ── Header ───────────────────────────────────────────────────────────

    Rectangle {
        id: headerBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: VLCStyle.dp(48, VLCStyle.scale)
        color: theme.bg.primary

        // Subtle bottom shadow
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: VLCStyle.dp(1, VLCStyle.scale)
            color: theme.border
            opacity: 0.25
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_normal
            anchors.rightMargin: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_xsmall

            Widgets.IconLabel {
                text: VLCIcons.eject
                font.pixelSize: VLCStyle.icon_normal
                color: theme.accent
            }

            Widgets.SubtitleLabel {
                Layout.fillWidth: true
                text: qsTr("Download Media")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_large
                font.bold: true
                leftPadding: VLCStyle.margin_xxsmall
                verticalAlignment: Text.AlignVCenter
            }

            Widgets.IconToolButton {
                text: VLCIcons.stop
                font.pixelSize: VLCStyle.icon_small
                opacity: hovered ? 1.0 : 0.6
                Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }
                onClicked: root.requestClose()
                Accessible.description: qsTr("Close")
            }
        }
    }

    // ── Content ──────────────────────────────────────────────────────────

    Item {
        id: contentArea
        anchors.top: headerBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footerBar.top
        clip: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                id: mainLayout
                width: contentArea.width - VLCStyle.margin_normal * 2
                spacing: VLCStyle.margin_small

                // ── URL Input Section ──────────────────────────────────

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: urlInputLayout.implicitHeight + VLCStyle.margin_small * 2
                    color: theme.bg.secondary
                    radius: VLCStyle.dp(8, VLCStyle.scale)

                    // Subtle border
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: "transparent"
                        border.color: urlField.activeFocus ? theme.accent : theme.border
                    border.width: urlField.activeFocus ? VLCStyle.dp(1.5, VLCStyle.scale) : 0
                    opacity: 0.5
                    visible: urlField.activeFocus || false
                        Behavior on border.width { NumberAnimation { duration: VLCStyle.duration_veryShort } }
                        Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_veryShort } }
                    }

                    RowLayout {
                        id: urlInputLayout
                        anchors.fill: parent
                        anchors.margins: VLCStyle.margin_small
                        spacing: VLCStyle.margin_xsmall

                        Widgets.TextFieldExt {
                            id: urlField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Paste a YouTube or media URL here...")
                            font.pixelSize: VLCStyle.fontSize_normal
                            text: root._initialUrl

                            onAccepted: analyzeUrl()
                            enabled: !root._isAnalyzing
                        }

                        Widgets.ButtonExt {
                            id: analyzeBtn
                            text: root._isAnalyzing ? qsTr("Analyzing...") : qsTr("Analyze")
                            font.bold: true
                            enabled: urlField.text.length > 0 && !root._isAnalyzing

                            onClicked: analyzeUrl()
                        }
                    }
                }

                // ── Progress / Analysis State ──────────────────────────

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.dp(4, VLCStyle.scale)
                    visible: root._isAnalyzing
                    color: theme.border
                    radius: VLCStyle.dp(2, VLCStyle.scale)
                    opacity: 0.4

                    Behavior on visible {
                        SequentialAnimation {
                            PropertyAction { property: "visible" }
                            NumberAnimation { property: "opacity"; from: 0; to: 0.4; duration: VLCStyle.duration_short }
                        }
                    }

                    Rectangle {
                        id: progressBar
                        width: parent.width * _analysisProgress
                        height: parent.height
                        color: theme.accent
                        radius: VLCStyle.dp(2, VLCStyle.scale)

                        Behavior on width {
                            NumberAnimation { duration: VLCStyle.duration_short; easing.type: Easing.OutCubic }
                        }

                        property real _analysisProgress: 0.0

                        SequentialAnimation on _analysisProgress {
                            running: root._isAnalyzing
                            loops: Animation.Infinite

                            NumberAnimation {
                                from: 0.0; to: 0.75
                                duration: 2500
                                easing.type: Easing.InOutSine
                            }
                            NumberAnimation {
                                from: 0.75; to: 0.9
                                duration: 1500
                                easing.type: Easing.InOutSine
                            }
                            PauseAnimation { duration: 800 }
                            NumberAnimation {
                                from: 0.9; to: 0.75
                                duration: 800
                                easing.type: Easing.InOutSine
                            }
                        }
                    }
                }

                // ── Media Info Section (visible after analysis) ────────

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: infoLayout.implicitHeight + VLCStyle.margin_normal * 2
                    visible: root._analysisComplete
                    color: theme.bg.secondary
                    radius: VLCStyle.dp(8, VLCStyle.scale)
                    border.color: Qt.rgba(theme.border.r, theme.border.g, theme.border.b, 0.15)

                    Behavior on visible {
                        SequentialAnimation {
                            PropertyAction { property: "visible" }
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: VLCStyle.duration_long }
                        }
                    }

                    RowLayout {
                        id: infoLayout
                        anchors.fill: parent
                        anchors.margins: VLCStyle.margin_normal
                        spacing: VLCStyle.margin_normal

                        // Thumbnail with rounded corners
                        Rectangle {
                            Layout.preferredWidth: VLCStyle.dp(160, VLCStyle.scale)
                            Layout.preferredHeight: VLCStyle.dp(90, VLCStyle.scale)
                            color: theme.border
                            radius: VLCStyle.dp(6, VLCStyle.scale)
                            clip: true

                            Widgets.IconLabel {
                                anchors.centerIn: parent
                                text: VLCIcons.album_cover
                                font.pixelSize: VLCStyle.icon_medium
                                color: theme.fg.secondary
                                opacity: 0.35
                            }

                            Image {
                                id: thumbnailImage
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                source: root._thumbnailUrl
                                visible: status === Image.Ready

                                Behavior on opacity {
                                    NumberAnimation { duration: VLCStyle.duration_short }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: VLCStyle.margin_xxxsmall

                            Widgets.ListLabel {
                                Layout.fillWidth: true
                                text: root._videoTitle
                                color: theme.fg.primary
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.bold: true
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                lineHeight: 1.3
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: VLCStyle.margin_xxxsmall
                                spacing: VLCStyle.margin_small

                                Widgets.CaptionLabel {
                                    text: root._videoDuration
                                    color: theme.accent
                                    font.pixelSize: VLCStyle.fontSize_small
                                    font.bold: true
                                }

                                Widgets.CaptionLabel {
                                    text: qsTr("by %1").arg(root._videoUploader)
                                    color: theme.fg.secondary
                                    font.pixelSize: VLCStyle.fontSize_small
                                    visible: root._videoUploader !== ""
                                }
                            }

                            Widgets.CaptionLabel {
                                Layout.fillWidth: true
                                Layout.topMargin: VLCStyle.margin_xxsmall
                                text: root._videoDescription
                                color: theme.fg.secondary
                                font.pixelSize: VLCStyle.fontSize_small
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                lineHeight: 1.3
                                opacity: 0.85
                            }
                        }
                    }
                }

                // ── Options Section (analysis complete) ─────────────────

                Rectangle {
                    Layout.fillWidth: true
                    visible: root._analysisComplete
                    color: "transparent"

                    Behavior on visible {
                        SequentialAnimation {
                            PropertyAction { property: "visible" }
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: VLCStyle.duration_long }
                        }
                    }

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: VLCStyle.margin_small

                        // Audio-only and embed toggles
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: checkboxRow.implicitHeight + VLCStyle.margin_small * 2
                            color: theme.bg.secondary
                            radius: VLCStyle.dp(8, VLCStyle.scale)

                            RowLayout {
                                id: checkboxRow
                                anchors.fill: parent
                                anchors.margins: VLCStyle.margin_small
                                spacing: VLCStyle.margin_normal

                                Widgets.CheckBoxExt {
                                    id: audioOnlyCheckbox
                                    text: qsTr("Audio only")
                                    checked: root._audioOnly
                                    font.pixelSize: VLCStyle.fontSize_normal
                                    onClicked: {
                                        root._audioOnly = checked
                                        audioSelector.audioOnly = checked
                                    }
                                }

                                Widgets.CheckBoxExt {
                                    id: embedMetadataCheckbox
                                    text: qsTr("Embed metadata")
                                    checked: true
                                    font.pixelSize: VLCStyle.fontSize_normal
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }

                        // Format selectors
                        QualitySelector {
                            id: qualitySelector
                            Layout.fillWidth: true
                            Layout.preferredHeight: VLCStyle.dp(200, VLCStyle.scale)
                            visible: root._analysisComplete && !root._audioOnly

                            Behavior on visible {
                                SequentialAnimation {
                                    PropertyAction { property: "visible" }
                                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: VLCStyle.duration_short }
                                }
                            }

                            onFormatSelected: function(idx) {
                                root._selectedFormatIdx = idx
                            }
                        }

                        AudioSelector {
                            id: audioSelector
                            Layout.fillWidth: true
                            Layout.preferredHeight: VLCStyle.dp(180, VLCStyle.scale)

                            audioOnly: root._audioOnly

                            onFormatSelected: function(idx) {
                                root._selectedFormatIdx = idx
                            }
                        }

                        SubtitleSelector {
                            id: subtitleSelector
                            Layout.fillWidth: true
                            Layout.preferredHeight: VLCStyle.dp(180, VLCStyle.scale)

                            onSubtitlesChanged: function(indices) {
                                root._selectedSubs = indices
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Footer ───────────────────────────────────────────────────────────

    Rectangle {
        id: footerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: VLCStyle.dp(56, VLCStyle.scale)
        color: theme.bg.primary

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: VLCStyle.dp(1, VLCStyle.scale)
            color: theme.border
            opacity: 0.25
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_normal
            anchors.rightMargin: VLCStyle.margin_normal
            spacing: VLCStyle.margin_xsmall

            // Save-to preview on the left
            Widgets.CaptionLabel {
                id: saveToPreview
                Layout.fillWidth: true
                text: ""
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                elide: Text.ElideRight
                visible: text !== ""
                opacity: visible ? 0.85 : 0
                verticalAlignment: Text.AlignVCenter
            }

            Widgets.ButtonExt {
                text: qsTr("Cancel")
                onClicked: {
                    if (root._currentTaskId !== "")
                        DownloaderController.cancelTask(root._currentTaskId)
                    root.requestClose()
                }
            }

            Widgets.ButtonExt {
                id: downloadBtn
                text: qsTr("Download")
                font.bold: true
                font.pixelSize: VLCStyle.fontSize_normal
                enabled: root._analysisComplete
                highlighted: true

                onClicked: startDownload()
            }
        }
    }

    // ── Functions ────────────────────────────────────────────────────────

    /**
     * @brief Analyze the URL entered by the user.
     */
    function analyzeUrl() {
        var url = urlField.text.trim()
        if (url === "")
            return

        root._isAnalyzing = true
        root._analysisComplete = false
        root._formatsLoaded = false

        root._currentTaskId = DownloaderController.createTask(url)
        if (root._currentTaskId === "") {
            root._isAnalyzing = false
            return
        }

        DownloaderController.analyzeTask(root._currentTaskId)

        pollTimer._elapsed = 0
        pollTimer.start()
    }

    Timer {
        id: pollTimer
        interval: 500
        repeat: true

        property int _elapsed: 0
        readonly property int _timeout: 120

        onTriggered: {
            _elapsed++

            if (root._currentTaskId === "") {
                console.warn("DownloadDialog: poll timer stopped — no task ID")
                pollTimer.stop()
                root._isAnalyzing = false
                return
            }

            if (_elapsed >= _timeout) {
                console.warn("DownloadDialog: analysis timed out for", root._currentTaskId)
                pollTimer.stop()
                root._isAnalyzing = false
                return
            }

            var info = DownloaderController.mediaInfoForTask(root._currentTaskId)
            if (info === undefined || info === null || info["title"] === undefined)
                return

            pollTimer.stop()
            onAnalysisComplete(info)
        }
    }

    Component.onCompleted: {
        if (root._initialUrl !== "") {
            analyzeUrl()
        }
    }

    function onAnalysisComplete(info) {
        if (root._formatsLoaded)
            return

        root._isAnalyzing = false
        root._analysisComplete = true
        root._formatsLoaded = true

        root._videoTitle = info["title"] || root._currentTaskId
        root._videoDuration = info["duration"] || ""
        root._videoUploader = info["uploader"] || ""
        root._videoDescription = info["description"] || ""
        root._thumbnailUrl = info["thumbnailUrl"] || ""

        qualitySelector.loadFromTask(root._currentTaskId)
        audioSelector.loadFromTask(root._currentTaskId)
        subtitleSelector.loadFromTask(root._currentTaskId)

        // Update save-to preview with filename
        var fileName = (root._videoTitle || "download") + (root._audioOnly ? ".m4a" : ".mp4")
        saveToPreview.text = qsTr("Save to: %1").arg(fileName)
    }

    function startDownload() {
        if (root._currentTaskId === "")
            return

        DownloaderController.confirmDownload(
            root._currentTaskId,
            root._selectedFormatIdx,
            root._audioOnly
        )

        root.requestClose()
    }
}
