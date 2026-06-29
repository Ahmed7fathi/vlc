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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "thumbnail_generator.hpp"
#include "thumbnail_cache.hpp"
#include "shared_input_item.hpp"
#include "qt.hpp"

#include <vlc_cxx_helpers.hpp>
#include <vlc_picture.h>
#include <vlc_block.h>

#include <QImage>
#include <QThread>
#include <QMutexLocker>

static const struct vlc_thumbnailer_cbs sm_thumbnailerCbs = [] {
    struct vlc_thumbnailer_cbs cbs{};
    cbs.on_ended = ThumbnailGenerator::onThumbnailEnded;
    return cbs;
}();

ThumbnailGenerator::ThumbnailGenerator(qt_intf_t *p_intf, ThumbnailCache *cache, QObject *parent)
    : QObject(parent)
    , m_p_intf(p_intf)
{
    if (cache)
    {
        m_cache = cache;
    }
    else
    {
        m_cache = new ThumbnailCache(this);
        m_ownsCache = true;
    }

    // Create preparser for thumbnail generation
    const struct vlc_preparser_cfg cfg = [] {
        struct vlc_preparser_cfg cfg{};
        cfg.types = VLC_PREPARSER_TYPE_THUMBNAIL;
        cfg.max_parser_threads = 0;
        cfg.max_thumbnailer_threads = 2;
        cfg.timeout = VLC_TICK_FROM_SEC(30);
        cfg.external_process = false;
        return cfg;
    }();

    m_preparser = vlc_preparser_New(VLC_OBJECT(p_intf), &cfg);
    if (!m_preparser)
        msg_Warn(p_intf, "Failed to create thumbnail preparser");
}

ThumbnailGenerator::~ThumbnailGenerator()
{
    cancelAll();

    if (m_preparser)
        vlc_preparser_Delete(m_preparser);
}

void ThumbnailGenerator::setCache(ThumbnailCache *cache)
{
    if (m_ownsCache && m_cache)
        delete m_cache;

    m_cache = cache;
    m_ownsCache = false;
}

bool ThumbnailGenerator::requestThumbnail(input_item_t *inputItem, const QString &mediaHash,
                                          int position, bool precise)
{
    if (!inputItem)
        return false;

    // Check cache first
    if (m_cache && m_cache->hasThumbnail(mediaHash, position))
    {
        QImage cachedImage = m_cache->getThumbnail(mediaHash, position);
        if (!cachedImage.isNull())
        {
            emit thumbnailReady(mediaHash, position, cachedImage);
            return true;
        }
    }

    // Don't generate if already in-flight
    {
        QMutexLocker lock(&m_mutex);
        if (m_inFlightRequests.contains(mediaHash) &&
            m_inFlightRequests[mediaHash].contains(position))
        {
            return true;
        }
    }

    // Queue the request (hold reference to input item)
    {
        QMutexLocker lock(&m_mutex);
        ThumbnailRequest req;
        req.inputItem = SharedInputItem(inputItem);
        req.mediaHash = mediaHash;
        req.position = position;
        req.precise = precise;
        m_pendingRequests.enqueue(req);
    }

    // Process the batch
    processNextBatch();

    return true;
}

void ThumbnailGenerator::cancelRequests(const QString &mediaHash)
{
    QMutexLocker lock(&m_mutex);

    // Remove pending requests
    QQueue<ThumbnailRequest> remaining;
    while (!m_pendingRequests.isEmpty())
    {
        ThumbnailRequest req = m_pendingRequests.dequeue();
        if (req.mediaHash != mediaHash)
            remaining.enqueue(req);
    }
    m_pendingRequests = remaining;

    m_inFlightRequests.remove(mediaHash);

    // Cancel current task if it matches
    if (m_currentRequest && m_currentRequest->mediaHash == mediaHash && m_preparser && m_currentTask)
    {
        vlc_preparser_Cancel(m_preparser, m_currentTask);
        m_currentTask = nullptr;
        delete m_currentRequest;
        m_currentRequest = nullptr;
    }
}

void ThumbnailGenerator::cancelAll()
{
    QMutexLocker lock(&m_mutex);
    m_pendingRequests.clear();
    m_inFlightRequests.clear();

    if (m_preparser && m_currentTask)
    {
        vlc_preparser_Cancel(m_preparser, m_currentTask);
        m_currentTask = nullptr;
    }

    if (m_currentRequest)
    {
        delete m_currentRequest;
        m_currentRequest = nullptr;
    }
}

bool ThumbnailGenerator::hasPendingRequests() const
{
    QMutexLocker lock(&m_mutex);
    return !m_pendingRequests.isEmpty() || m_currentRequest != nullptr;
}

void ThumbnailGenerator::processNextBatch()
{
    // Extract info under lock, process VLC call without lock
    ThumbnailRequest *request = nullptr;
    bool canProcess = false;

    {
        QMutexLocker lock(&m_mutex);

        if (m_currentRequest || m_pendingRequests.isEmpty())
            return;

        if (!m_preparser)
        {
            msg_Warn(m_p_intf, "Thumbnail preparser not available");
            return;
        }

        // Dequeue the next request
        m_currentRequest = new ThumbnailRequest(m_pendingRequests.dequeue());

        // Track it
        if (!m_inFlightRequests.contains(m_currentRequest->mediaHash))
            m_inFlightRequests[m_currentRequest->mediaHash] = QList<int>();
        m_inFlightRequests[m_currentRequest->mediaHash].append(m_currentRequest->position);

        request = m_currentRequest;
        canProcess = true;
    }

    if (!canProcess || !request)
        return;

    // Set up the thumbnail argument (no lock needed)
    struct vlc_thumbnailer_arg arg = {};
    arg.seek.type = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_TIME;
    arg.seek.time = VLC_TICK_FROM_SEC(request->position);
    arg.seek.speed = request->precise
        ? vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_PRECISE
        : vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_FAST;
    arg.hw_dec = true;

    vlc_preparser_req *task = vlc_preparser_GenerateThumbnail(
        m_preparser,
        request->inputItem.get(),
        &arg,
        &sm_thumbnailerCbs,
        this
    );

    if (!task)
    {
        msg_Warn(m_p_intf, "Failed to queue thumbnail generation for position %d",
                 request->position);

        QString mediaHash = request->mediaHash;
        int position = request->position;

        {
            QMutexLocker lock(&m_mutex);
            delete m_currentRequest;
            m_currentRequest = nullptr;
            if (m_inFlightRequests.contains(mediaHash))
            {
                m_inFlightRequests[mediaHash].removeAll(position);
                if (m_inFlightRequests[mediaHash].isEmpty())
                    m_inFlightRequests.remove(mediaHash);
            }
        }

        emit thumbnailFailed(mediaHash, position, "Failed to queue thumbnail request");

        // Try next batch
        processNextBatch();
    }
    else
    {
        QMutexLocker lock(&m_mutex);
        m_currentTask = task;
    }
}

void ThumbnailGenerator::onThumbnailEnded(vlc_preparser_req *req, int status,
                                           picture_t *thumbnail, void *data)
{
    ThumbnailGenerator *self = static_cast<ThumbnailGenerator *>(data);
    self->handleThumbnailResult(req, status, thumbnail);
}

void ThumbnailGenerator::handleThumbnailResult(vlc_preparser_req *req, int status,
                                                 picture_t *thumbnail)
{
    // Extract data under lock, then process outside the lock
    QString mediaHash;
    int position = 0;

    {
        QMutexLocker lock(&m_mutex);

        if (!m_currentRequest || m_currentTask != req)
        {
            /* Stale callback for a request we already cancelled/processed.
             * Per VLC API contract: The picture is owned by the thumbnailer,
             * we MUST NOT call picture_Release on it. Just release the req. */
            lock.unlock();
            vlc_preparser_req_Release(req);
            return;
        }

        m_currentTask = nullptr;
        mediaHash = m_currentRequest->mediaHash;
        position = m_currentRequest->position;
    }

    if (status == VLC_SUCCESS && thumbnail)
    {
        // Convert picture to QImage (deep copy while picture is alive)
        QImage image = pictureToQImage(thumbnail);

        /* Per VLC API contract (vlc_preparser.h): The picture is owned by
         * the thumbnailer. We MUST NOT call picture_Release() — VLC will
         * clean it up after the callback returns. */

        if (!image.isNull())
        {
            // Cache the thumbnail (may block briefly but not holding mutex)
            if (m_cache)
                m_cache->putThumbnail(mediaHash, position, image);

            emit thumbnailReady(mediaHash, position, image);
        }
        else
        {
            msg_Warn(m_p_intf, "Failed to convert thumbnail picture to QImage for pos %d",
                     position);
            emit thumbnailFailed(mediaHash, position,
                                 "Failed to convert picture to image");
        }
    }
    else
    {
        QString errorMsg;
        switch (status)
        {
        case VLC_ETIMEOUT:
            errorMsg = "Thumbnail generation timed out";
            break;
        case -EINTR:
            errorMsg = "Thumbnail generation cancelled";
            break;
        default:
            errorMsg = "Thumbnail generation failed";
            break;
        }

        emit thumbnailFailed(mediaHash, position, errorMsg);
    }

    // Cleanup current request
    {
        QMutexLocker lock(&m_mutex);
        bool batchDone = false;
        if (m_inFlightRequests.contains(mediaHash))
        {
            m_inFlightRequests[mediaHash].removeAll(position);
            if (m_inFlightRequests[mediaHash].isEmpty())
            {
                m_inFlightRequests.remove(mediaHash);
                batchDone = true;
            }
        }

        delete m_currentRequest;
        m_currentRequest = nullptr;

        if (batchDone)
        {
            lock.unlock();
            int succeeded = (status == VLC_SUCCESS) ? 1 : 0;
            emit batchComplete(mediaHash, 1, succeeded);
            lock.relock();
        }
    }

    vlc_preparser_req_Release(req);

    // Process next batch (without holding m_mutex)
    processNextBatch();
}

QImage ThumbnailGenerator::pictureToQImage(picture_t *picture) const
{
    if (!picture)
        return QImage();

    /* Use picture_Export() to convert any picture format (I420, NV12, etc.)
     * to PNG in memory, then load into QImage.
     *
     * picture_Export() takes a VLC object pointer for logging, the output
     * block pointer, an optional format hint, the source picture, the
     * desired codec (PNG), and optional width/height override. */
    block_t *block = NULL;
    int ret = picture_Export(VLC_OBJECT(m_p_intf), &block, NULL,
                             picture, VLC_CODEC_PNG,
                             m_thumbnailWidth, 0, false);
    if (ret == VLC_SUCCESS && block)
    {
        // Load the PNG data into QImage
        QImage image = QImage::fromData(
            reinterpret_cast<const uchar *>(block->p_buffer),
            static_cast<int>(block->i_buffer),
            "PNG"
        );
        block_Release(block);

        // Deep copy to ensure our QImage owns its pixel data independently
        return image.copy();
    }

    /* picture_Export failed — try a direct I420 software conversion.
     * I420 is the most common video output format and the software path
     * avoids depending on the PNG encoder module being available. */
    if (block)
        block_Release(block);

    if (picture->format.i_chroma != VLC_CODEC_I420)
    {
        msg_Warn(m_p_intf, "picture_Export failed (ret=%d) for unsupported chroma %4.4s",
                 ret, reinterpret_cast<const char *>(&picture->format.i_chroma));
        return QImage();
    }

    return convertI420ToQImage(picture);
}

QImage ThumbnailGenerator::convertI420ToQImage(picture_t *picture) const
{
    const video_format_t *fmt = &picture->format;
    int srcW = fmt->i_visible_width;
    int srcH = fmt->i_visible_height;

    if (srcW <= 0 || srcH <= 0)
        return QImage();

    // Scale proportionally to the configured thumbnail width
    int dstW = m_thumbnailWidth;
    int dstH = (srcH * m_thumbnailWidth + srcW / 2) / srcW;
    // Keep dimensions even for YUV chroma subsampling compatibility
    if (dstH % 2 != 0)
        dstH++;

    QImage image(dstW, dstH, QImage::Format_RGB888);
    if (image.isNull())
        return QImage();

    const uint8_t *yPlane = picture->p[0].p_pixels;
    const uint8_t *uPlane = picture->p[1].p_pixels;
    const uint8_t *vPlane = picture->p[2].p_pixels;
    const int yPitch = picture->p[0].i_pitch;
    const int uvPitch = picture->p[1].i_pitch;

    // BT.601 limited-range YUV → RGB conversion
    // Derived from the standard matrix:
    //   R = 1.164(Y-16) + 1.596(V-128)
    //   G = 1.164(Y-16) - 0.391(U-128) - 0.813(V-128)
    //   B = 1.164(Y-16) + 2.018(U-128)
    // Implemented with 8-bit fixed-point (×256) for speed.
    for (int y = 0; y < dstH; y++)
    {
        int srcY = (y * srcH) / dstH;
        uint8_t *scanline = image.scanLine(y);

        for (int x = 0; x < dstW; x++)
        {
            int srcX = (x * srcW) / dstW;

            int Y = yPlane[srcY * yPitch + srcX];
            int U = uPlane[(srcY / 2) * uvPitch + (srcX / 2)];
            int V = vPlane[(srcY / 2) * uvPitch + (srcX / 2)];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = (298 * C + 409 * E + 128) >> 8;
            int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int B = (298 * C + 516 * D + 128) >> 8;

            scanline[x * 3 + 0] = static_cast<uint8_t>(qBound(0, R, 255));
            scanline[x * 3 + 1] = static_cast<uint8_t>(qBound(0, G, 255));
            scanline[x * 3 + 2] = static_cast<uint8_t>(qBound(0, B, 255));
        }
    }

    return image;
}
