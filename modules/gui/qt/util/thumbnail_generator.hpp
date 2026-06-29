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

#ifndef THUMBNAIL_GENERATOR_HPP
#define THUMBNAIL_GENERATOR_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QString>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QTimer>

#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_input_item.h>
#include <vlc_picture.h>
#include "util/shared_input_item.hpp"

struct qt_intf_t;

class ThumbnailCache;

/**
 * @brief ThumbnailGenerator asynchronously generates video thumbnails using VLC's preparser API.
 *
 * This class manages a queue of thumbnail generation requests. Each request
 * specifies a media (via input_item_t) and a position. Thumbnails are generated
 * in background threads and reported back via signals. Generated thumbnails are
 * automatically stored in the ThumbnailCache.
 *
 * Multiple positions for the same media are batched together to minimize the
 * number of separate decoding sessions.
 */
class ThumbnailGenerator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a ThumbnailGenerator
     * @param p_intf VLC interface handle
     * @param cache Pointer to ThumbnailCache instance (optional)
     * @param parent Parent QObject
     */
    explicit ThumbnailGenerator(qt_intf_t *p_intf, ThumbnailCache *cache = nullptr,
                                QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ThumbnailGenerator() override;

    /**
     * @brief Set the ThumbnailCache instance
     * @param cache Pointer to ThumbnailCache
     */
    void setCache(ThumbnailCache *cache);

    /**
     * @brief Request a thumbnail for the given media at the specified position
     *
     * If the thumbnail is already cached, the thumbnailReady signal is emitted
     * immediately. Otherwise, it is generated asynchronously.
     *
     * @param inputItem The input item (media) to generate a thumbnail for.
     *                  The caller retains ownership.
     * @param mediaHash A unique hash string identifying this media
     * @param position Position in seconds
     * @param precise If true, use VLC_THUMBNAILER_SEEK_PRECISE (decodes all frames
     *                up to the target). If false (default), uses _SEEK_FAST which
     *                jumps to the nearest keyframe.
     * @return true if the request was queued (or served from cache)
     */
    bool requestThumbnail(input_item_t *inputItem, const QString &mediaHash,
                          int position, bool precise = false);

    /**
     * @brief Cancel all pending requests for the given media
     * @param mediaHash The media hash to cancel requests for
     */
    void cancelRequests(const QString &mediaHash);

    /**
     * @brief Cancel all pending requests
     */
    void cancelAll();

    /**
     * @brief Check if there are pending requests
     * @return true if requests are queued or in progress
     */
    bool hasPendingRequests() const;

    /**
     * @brief Get the thumbnail interval in seconds
     * @return Interval in seconds
     */
    int interval() const { return m_interval; }

    /**
     * @brief Set the thumbnail interval in seconds
     * @param sec Interval in seconds
     */
    void setInterval(int sec) { m_interval = sec; }

    /**
     * @brief Get target thumbnail width
     * @return Width in pixels
     */
    int thumbnailWidth() const { return m_thumbnailWidth; }

    /**
     * @brief Set target thumbnail width
     * @param width Width in pixels
     */
    void setThumbnailWidth(int width) { m_thumbnailWidth = width; }

    /**
     * @brief Get target thumbnail height
     * @return Height in pixels
     */
    int thumbnailHeight() const { return m_thumbnailHeight; }

    /**
     * @brief Set target thumbnail height
     * @param height Height in pixels
     */
    void setThumbnailHeight(int height) { m_thumbnailHeight = height; }

signals:
    /**
     * @brief Emitted when a thumbnail is ready
     * @param mediaHash The media hash
     * @param position Position in seconds
     * @param image The generated thumbnail image
     */
    void thumbnailReady(const QString &mediaHash, int position, const QImage &image);

    /**
     * @brief Emitted when a thumbnail generation failed
     * @param mediaHash The media hash
     * @param position Position in seconds
     * @param errorMessage Description of the error
     */
    void thumbnailFailed(const QString &mediaHash, int position, const QString &errorMessage);

    /**
     * @brief Emitted when batch generation for a media is complete
     * @param mediaHash The media hash
     * @param total Total number of thumbnails
     * @param succeeded Number of successfully generated thumbnails
     */
    void batchComplete(const QString &mediaHash, int total, int succeeded);

public:
    /**
     * @brief Callback for thumbnail generation completion
     *
     * Public because it's used as a C callback from the VLC preparser API
     * via vlc_thumbnailer_cbs.
     */
    static void onThumbnailEnded(vlc_preparser_req *req, int status,
                                  picture_t *thumbnail, void *data);

    /**
     * @brief Handle thumbnail generation result on the main thread
     *
     * Public because it's called from the static onThumbnailEnded callback.
     */
    void handleThumbnailResult(vlc_preparser_req *req, int status,
                                picture_t *thumbnail);

private:
    struct ThumbnailRequest
    {
        SharedInputItem inputItem; // Held reference until completion
        QString mediaHash;
        int position;
        bool precise = false; // Use VLC_THUMBNAILER_SEEK_PRECISE vs _SEEK_FAST
    };

    /**
     * @brief Process the next batch of requests
     */
    void processNextBatch();

    /**
     * @brief Convert a VLC picture_t to a QImage
     */
    QImage pictureToQImage(picture_t *picture) const;

    /**
     * @brief Direct software I420→RGB888 conversion (fallback if picture_Export fails)
     */
    QImage convertI420ToQImage(picture_t *picture) const;

    qt_intf_t *m_p_intf;
    ThumbnailCache *m_cache = nullptr;
    bool m_ownsCache = false;

    vlc_preparser_t *m_preparser = nullptr;

    QQueue<ThumbnailRequest> m_pendingRequests;
    ThumbnailRequest *m_currentRequest = nullptr;
    vlc_preparser_req *m_currentTask = nullptr;
    mutable QMutex m_mutex;

    int m_interval = 5;           // Default 5 seconds
    int m_thumbnailWidth = 320;   // Default 320px
    int m_thumbnailHeight = 180;  // Default 16:9 aspect ratio

    // Track which positions are being generated for each media
    QHash<QString, QList<int>> m_inFlightRequests;

};

#endif // THUMBNAIL_GENERATOR_HPP
