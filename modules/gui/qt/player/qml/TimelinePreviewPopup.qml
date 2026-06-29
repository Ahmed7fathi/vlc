/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Templates as T

import VLC.Style
import VLC.Widgets as Widgets

/**
 * @brief A YouTube-style timeline preview popup that appears above the seek slider.
 *
 * Displays a video thumbnail at the hover position, along with a timestamp
 * overlay and optional chapter title. Features smooth enter/exit transitions,
 * a downward-pointing arrow pointer, and a shimmering play-icon placeholder.
 */
T.Popup {
    id: root

    /** Whether timeline previews are enabled */
    property bool previewEnabled: true

    /** URL of the thumbnail image to display */
    property url thumbnailSource: ""

    /** Timestamp text (e.g. "01:23:45") */
    property string timestamp: ""

    /** Chapter title at the current position */
    property string chapterTitle: ""

    /** Whether a chapter is active at this position */
    property bool hasChapter: false

    /** Width of the thumbnail area */
    property int thumbnailWidth: VLCStyle.dp(240, VLCStyle.scale)

    /** Maximum height for the thumbnail card (prevents extreme vertical ratios) */
    property int thumbnailMaxHeight: VLCStyle.dp(300, VLCStyle.scale)

    /** Minimum height for the thumbnail card (prevents extreme horizontal ratios) */
    property int thumbnailMinHeight: VLCStyle.dp(60, VLCStyle.scale)

    /**
     * Height of the thumbnail area — adapts to the actual image aspect ratio
     * when an image is loaded, falling back to 16:9 (thumbnailWidth / 1.778)
     * when no image is available.
     */
    property int thumbnailHeight: Math.max(thumbnailMinHeight,
        Math.min(Math.round(thumbnailWidth / _imageAspectRatio), thumbnailMaxHeight))

    /** Tracked aspect ratio of the currently loaded thumbnail image */
    property real _imageAspectRatio: 16.0 / 9.0

    /** Position of the popup along the slider (0.0 – 1.0) */
    property real previewPosition: 0.0

    /** Reference to the slider item for positioning */
    property Item sliderItem: null

    // ── Transitions ────────────────────────────────────────────────────

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: VLCStyle.duration_short
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "scale"
                from: 0.92
                to: 1.0
                duration: VLCStyle.duration_short
                easing.type: Easing.OutCubic
            }
        }
    }

    // No exit transition — the popup disappears instantly when the
    // mouse leaves the slider, avoiding any visible snap to the
    // playback cursor position during a fade-out animation.

    // ── Basic setup ────────────────────────────────────────────────────

    background: null
    closePolicy: Popup.NoAutoClose
    z: 100

    padding: 0
    margins: 0

    // Size based on content
    implicitWidth: contentWrapper.width
    implicitHeight: contentWrapper.height + arrowArea.implicitHeight

    // Positioning: above the slider, centered on the cursor position.
    // x/y are in the slider's local coordinate system (the popup's
    // QML parent). NO Behavior on x animation — the popup must track
    // the cursor instantly to avoid a "lagging behind" feel.
    x: {
        if (!sliderItem) return 0
        const sliderWidth = sliderItem.width
        const popupWidth = contentWrapper.width
        const pos = previewPosition * sliderWidth - popupWidth / 2
        // Clamp to prevent going off-screen
        return Math.max(VLCStyle.margin_xxsmall,
                        Math.min(pos, sliderWidth - popupWidth - VLCStyle.margin_xxsmall))
    }

    y: -(contentWrapper.height + arrowArea.implicitHeight + VLCStyle.margin_xxxsmall)

    // ── Main content column ────────────────────────────────────────────

    Column {
        id: contentWrapper

        spacing: 0

        // --- Thumbnail card with rounded corners and shadow ---
        Item {
            id: thumbnailCard

            width: root.thumbnailWidth
            height: root.thumbnailHeight

            // Layered shadow for depth
            Widgets.RoundedRectangleShadow {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.35)
                yOffset: VLCStyle.dp(4, VLCStyle.scale)
                blurRadius: VLCStyle.dp(12, VLCStyle.scale)
            }

            // Secondary, tighter shadow
            Widgets.RoundedRectangleShadow {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.15)
                yOffset: VLCStyle.dp(2, VLCStyle.scale)
                blurRadius: VLCStyle.dp(4, VLCStyle.scale)
            }

            // Thumbnail image clipped to rounded corners
            Rectangle {
                anchors.fill: parent
                radius: VLCStyle.dp(8, VLCStyle.scale)
                color: Qt.rgba(0.08, 0.08, 0.08, 1.0)
                clip: true

                // Single Image element — QML's Image keeps the old pixmap
                // displayed while loading a new source asynchronously, so
                // there's no flash or placeholder gap. The image simply
                // updates atomically when the new thumbnail is decoded.
                //
                // On load, the image's intrinsic aspect ratio is captured
                // and used to resize the popup to match the content.
                Image {
                    id: thumbnailImage

                    anchors.fill: parent
                    source: root.thumbnailSource
                    // Request source at 2x display width for retina clarity,
                    // but don't constrain height (0 = automatic)
                    // Request source at 2x display width for retina clarity;
                    // max height 4× width for tall content, 0 would be ideal
                    // ("no constraint") but some Qt versions treat 0 differently.
                    sourceSize: Qt.size(root.thumbnailWidth * 2, root.thumbnailWidth * 4)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false

                    // Detect the actual image aspect ratio on load
                    onStatusChanged: {
                        if (status === Image.Ready) {
                            var w = sourceSize.width
                            var h = sourceSize.height
                            if (w > 0 && h > 0)
                                root._imageAspectRatio = w / h
                        }
                    }
                }

                // ── Timestamp overlay bar at the bottom of the thumbnail ──
                Rectangle {
                    id: timestampBar

                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }

                    height: VLCStyle.dp(28, VLCStyle.scale)
                    radius: 0

                    // Gradient overlay for readability
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.0) }
                        GradientStop { position: 0.35; color: Qt.rgba(0, 0, 0, 0.45) }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.75) }
                    }

                    // Play icon + timestamp text
                    Row {
                        anchors {
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                            leftMargin: VLCStyle.margin_xxsmall
                            rightMargin: VLCStyle.margin_xxsmall
                            bottomMargin: VLCStyle.margin_xxxsmall
                        }

                        spacing: VLCStyle.margin_xxxsmall

                        Text {
                            id: playIconOverlay
                            text: VLCIcons.play_filled
                            font.family: VLCIcons.fontFamily
                            font.pixelSize: VLCStyle.dp(10, VLCStyle.scale)
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            id: timestampOverlay
                            text: root.timestamp
                            color: "white"
                            font.pixelSize: VLCStyle.fontSize_xsmall
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        // --- Chapter info row (visible only when a chapter exists) ---
        Rectangle {
            id: chapterRow

            anchors {
                left: thumbnailCard.left
                right: thumbnailCard.right
            }

            height: root.hasChapter && root.chapterTitle !== ""
                    ? VLCStyle.dp(26, VLCStyle.scale) : 0
            radius: VLCStyle.dp(8, VLCStyle.scale)
            color: Qt.rgba(0.12, 0.12, 0.12, 1.0)

            // Only round the bottom corners by layering a square top half
            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                height: parent.radius
                color: parent.color
            }

            visible: height > 0

            Row {
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: VLCStyle.margin_xxsmall
                    rightMargin: VLCStyle.margin_xxsmall
                    verticalCenter: parent.verticalCenter
                }

                spacing: VLCStyle.margin_xxxsmall

                Text {
                    text: VLCIcons.bookmark
                    font.family: VLCIcons.fontFamily
                    font.pixelSize: VLCStyle.fontSize_xsmall
                    color: Qt.rgba(1, 1, 1, 0.8)
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: root.chapterTitle
                    color: Qt.rgba(1, 1, 1, 0.8)
                    font.pixelSize: VLCStyle.fontSize_xsmall
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // ── Arrow pointer pointing down to the slider ──────────────────────

    Item {
        id: arrowArea

        anchors {
            top: contentWrapper.bottom
            horizontalCenter: parent.horizontalCenter
        }

        width: arrowSize
        height: arrowSize * 0.5

        readonly property real arrowSize: VLCStyle.dp(12, VLCStyle.scale)

        // Clip the bottom half so the rotated rectangle forms a triangle tip
        clip: true

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: -arrowSize * 0.15

            width: arrowSize
            height: arrowSize
            rotation: 45
            color: Qt.rgba(0.12, 0.12, 0.12, 1.0)
        }
    }

    // ── Behaviour helpers ──────────────────────────────────────────────

    onPreviewEnabledChanged: {
        if (!previewEnabled)
            close()
    }

    // No onOpened handler needed — the crossfader's onVisibleChanged
    // already loads the thumbnail source when the popup becomes visible.
}
