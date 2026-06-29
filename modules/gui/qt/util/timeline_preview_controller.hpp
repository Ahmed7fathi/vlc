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

#ifndef TIMELINE_PREVIEW_CONTROLLER_HPP
#define TIMELINE_PREVIEW_CONTROLLER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QString>
#include <QImage>
#include <QUrl>
#include <QTimer>
#include <QPointer>

#include <vlc_common.h>
#include <vlc_input_item.h>

struct qt_intf_t;

class ThumbnailCache;
class ThumbnailGenerator;
class PlayerController;

/**
 * @brief TimelinePreviewController bridges the C++ thumbnail system with QML.
 *
 * This class is exposed to QML and provides:
 * - The thumbnail image at the current hover position
 * - The timestamp at the current hover position
 * - Chapter title at the current hover position
 * - Settings for enabling/disabling the feature
 *
 * It manages the relationship between the SliderBar hover position and
 * the ThumbnailGenerator/ThumbnailCache backend.
 */
class TimelinePreviewController : public QObject
{
    Q_OBJECT

    /** Whether timeline previews are enabled */
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)

    /** The hover position as a fraction [0.0, 1.0] of the timeline */
    Q_PROPERTY(double hoverPosition READ hoverPosition WRITE setHoverPosition NOTIFY hoverPositionChanged FINAL)

    /** Whether the user is hovering over the timeline */
    Q_PROPERTY(bool hovering READ isHovering WRITE setHovering NOTIFY hoveringChanged FINAL)

    /** The thumbnail image at the current hover position (QML-friendly URL) */
    Q_PROPERTY(QUrl thumbnailUrl READ thumbnailUrl NOTIFY thumbnailUrlChanged FINAL)

    /** The timestamp string at the current hover position (e.g. "01:23:45") */
    Q_PROPERTY(QString timestampText READ timestampText NOTIFY timestampTextChanged FINAL)

    /** The chapter title at the current hover position */
    Q_PROPERTY(QString chapterTitle READ chapterTitle NOTIFY chapterTitleChanged FINAL)

    /** Whether the hover position falls within a chapter */
    Q_PROPERTY(bool hasChapter READ hasChapter NOTIFY hasChapterChanged FINAL)

    /** The thumbnail interval in seconds */
    Q_PROPERTY(int thumbnailInterval READ thumbnailInterval WRITE setThumbnailInterval NOTIFY thumbnailIntervalChanged FINAL)

    /** The total media length in seconds */
    Q_PROPERTY(double mediaLength READ mediaLength WRITE setMediaLength NOTIFY mediaLengthChanged FINAL)

public:
    /**
     * @brief Construct a TimelinePreviewController
     * @param p_intf VLC interface handle
     * @param playerController Pointer to the player controller for accessing media info
     * @param parent Parent QObject
     */
    explicit TimelinePreviewController(qt_intf_t *p_intf,
                                        PlayerController *playerController = nullptr,
                                        QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~TimelinePreviewController() override;

    // Property getters
    bool isEnabled() const;
    double hoverPosition() const;
    bool isHovering() const;
    QUrl thumbnailUrl() const;
    QString timestampText() const;
    QString chapterTitle() const;
    bool hasChapter() const;
    int thumbnailInterval() const;
    double mediaLength() const;

    // Property setters
    void setEnabled(bool enabled);
    void setHoverPosition(double position);
    void setHovering(bool hovering);
    void setThumbnailInterval(int interval);
    void setMediaLength(double lengthSec);

    /**
     * @brief Called when a new media is loaded to reset cached state
     * @param inputItem The new input item (may be null, will be fetched from PlayerController)
     * @param mediaHash Unique hash for the media (if empty, will be computed from current media URL)
     */
    Q_INVOKABLE void onMediaChanged(input_item_t *inputItem, const QString &mediaHash);

    /**
     * @brief Compute a hash from a media URL for cache key purposes
     * @param url The media URL
     * @return A hash string suitable for use as a cache key
     */
    Q_INVOKABLE static QString computeHash(const QString &url);

    /**
     * @brief Convenience method to reset for a new media with just a URL
     * @param url The media URL
     */
    Q_INVOKABLE void onMediaUrlChanged(const QString &url);

    /**
     * @brief Get the ThumbnailCache instance
     */
    ThumbnailCache *cache() const { return m_cache; }

    /**
     * @brief Get the ThumbnailGenerator instance
     */
    ThumbnailGenerator *generator() const { return m_generator; }

signals:
    void enabledChanged();
    void hoverPositionChanged();
    void hoveringChanged();
    void thumbnailUrlChanged();
    void timestampTextChanged();
    void chapterTitleChanged();
    void hasChapterChanged();
    void thumbnailIntervalChanged();
    void mediaLengthChanged();

private slots:
    /**
     * @brief Handle a newly generated thumbnail
     */
    void onThumbnailReady(const QString &mediaHash, int position, const QImage &image);

    /**
     * @brief Update chapter information based on current hover position
     */
    void updateChapterInfo();

private:
    /**
     * @brief Pre-generate thumbnails for the entire timeline
     */
    void pregenerateThumbnails();

    /**
     * @brief Queue the next batch of pregeneration thumbnails.
     *
     * Progressively feeds positions to the generator (a few at a time)
     * so the preparser never has a huge backlog on long videos.
     */
    void queueNextPregenerationBatch();

    /**
     * @brief Compute seconds from hover position
     */
    int positionToSeconds() const;

    qt_intf_t *m_p_intf;
    QPointer<PlayerController> m_playerController;

    ThumbnailCache *m_cache = nullptr;
    ThumbnailGenerator *m_generator = nullptr;

    bool m_enabled = false;
    double m_hoverPosition = 0.0;
    bool m_hovering = false;
    double m_mediaLength = 0.0;
    int m_interval = 5;

    QString m_currentMediaHash;
    QUrl m_currentThumbnailUrl;
    QString m_timestampText;
    QString m_chapterTitle;
    bool m_hasChapter = false;

    // Debounce timer for thumbnail requests while hovering
    QTimer m_debounceTimer;
    int m_lastRequestedExactSecond = -1;

    // Debounce timer for QML URL updates to prevent rapid flickering
    QTimer m_urlDebounceTimer;
    QUrl m_pendingThumbnailUrl;

    // Progressive pregeneration state — keeps the preparser from being
    // flooded with hundreds of simultaneous thumbnail requests on long videos
    int m_pregenerationNextIndex = 0;
    int m_pregenerationTotal = 0;
};

#endif // TIMELINE_PREVIEW_CONTROLLER_HPP
