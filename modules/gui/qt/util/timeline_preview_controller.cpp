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

#include "timeline_preview_controller.hpp"
#include "thumbnail_cache.hpp"
#include "thumbnail_generator.hpp"
#include "player/player_controller.hpp"
#include "player/input_models.hpp"
#include "qt.hpp"

#include <vlc_configuration.h>
#include <vlc_cxx_helpers.hpp>

#include <QCryptographicHash>
#include <QFileInfo>
#include <QUrl>
#include <QFile>
#include <QDir>

TimelinePreviewController::TimelinePreviewController(qt_intf_t *p_intf,
                                                       PlayerController *playerController,
                                                       QObject *parent)
    : QObject(parent)
    , m_p_intf(p_intf)
    , m_playerController(playerController)
{
    // Load settings
    m_enabled = var_InheritBool(p_intf, "timeline-thumbnail-enabled");
    m_interval = config_GetInt("timeline-thumbnail-interval");
    if (m_interval <= 0)
        m_interval = 5;

    // Create cache and generator
    m_cache = new ThumbnailCache(this);
    m_generator = new ThumbnailGenerator(p_intf, m_cache, this);

    // Connect generator signals
    connect(m_generator, &ThumbnailGenerator::thumbnailReady,
            this, &TimelinePreviewController::onThumbnailReady);

    // On thumbnail failure, advance pregeneration so failures don't stall it
    connect(m_generator, &ThumbnailGenerator::thumbnailFailed,
            this, [this]() {
        queueNextPregenerationBatch();
    });

    // Setup debounce timer for hover-based thumbnail requests
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(150); // 150ms debounce
    connect(&m_debounceTimer, &QTimer::timeout, this, [this]() {
        if (m_hovering && m_enabled && !m_currentMediaHash.isEmpty())
        {
            int seconds = positionToSeconds();
            input_item_t *inputItem = m_playerController
                ? m_playerController->getInput() : nullptr;

            if (inputItem && seconds >= 0)
            {
                // Use precise seek for hover requests so the thumbnail
                // matches the exact frame at the cursor position
                m_generator->requestThumbnail(inputItem, m_currentMediaHash,
                                              seconds, /*precise=*/true);
            }
        }
    });

    // Setup URL debounce timer to prevent rapid QML Image flickering
    // when multiple thumbnail completions arrive close together
    m_urlDebounceTimer.setSingleShot(true);
    m_urlDebounceTimer.setInterval(50); // 50ms debounce before updating QML URL
    connect(&m_urlDebounceTimer, &QTimer::timeout, this, [this]() {
        if (!m_pendingThumbnailUrl.isEmpty())
        {
            m_currentThumbnailUrl = m_pendingThumbnailUrl;
            m_pendingThumbnailUrl = QUrl();
            emit thumbnailUrlChanged();
        }
    });
}

TimelinePreviewController::~TimelinePreviewController()
{
}

// Property getters

bool TimelinePreviewController::isEnabled() const
{
    return m_enabled;
}

double TimelinePreviewController::hoverPosition() const
{
    return m_hoverPosition;
}

bool TimelinePreviewController::isHovering() const
{
    return m_hovering;
}

QUrl TimelinePreviewController::thumbnailUrl() const
{
    return m_currentThumbnailUrl;
}

QString TimelinePreviewController::timestampText() const
{
    return m_timestampText;
}

QString TimelinePreviewController::chapterTitle() const
{
    return m_chapterTitle;
}

bool TimelinePreviewController::hasChapter() const
{
    return m_hasChapter;
}

int TimelinePreviewController::thumbnailInterval() const
{
    return m_interval;
}

double TimelinePreviewController::mediaLength() const
{
    return m_mediaLength;
}

// Property setters

void TimelinePreviewController::setEnabled(bool enabled)
{
    if (m_enabled != enabled)
    {
        m_enabled = enabled;
        config_PutInt("timeline-thumbnail-enabled", enabled ? 1 : 0);
        emit enabledChanged();
    }
}

void TimelinePreviewController::setHoverPosition(double position)
{
    position = qBound(0.0, position, 1.0);

    if (qFuzzyCompare(m_hoverPosition, position))
        return;

    m_hoverPosition = position;

    // Update timestamp text
    if (m_mediaLength > 0)
    {
        double totalSeconds = m_mediaLength * position;
        int hours = static_cast<int>(totalSeconds) / 3600;
        int minutes = (static_cast<int>(totalSeconds) % 3600) / 60;
        int secs = static_cast<int>(totalSeconds) % 60;

        if (hours > 0)
            m_timestampText = QString("%1:%2:%3")
                .arg(hours, 2, 10, QLatin1Char('0'))
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(secs, 2, 10, QLatin1Char('0'));
        else
            m_timestampText = QString("%1:%2")
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(secs, 2, 10, QLatin1Char('0'));

        emit timestampTextChanged();
    }

    // Update chapter info
    updateChapterInfo();

    // Request thumbnail for the exact hovered position (no interval snapping)
    int seconds = positionToSeconds();
    if (m_hovering && m_enabled && !m_currentMediaHash.isEmpty() && seconds >= 0)
    {
        // Only request if this is a different position than the last request
        if (seconds != m_lastRequestedExactSecond)
        {
            m_lastRequestedExactSecond = seconds;
            m_debounceTimer.start();
        }
    }

    emit hoverPositionChanged();
}

void TimelinePreviewController::setHovering(bool hovering)
{
    if (m_hovering != hovering)
    {
        m_hovering = hovering;

        if (!hovering)
        {
            // Clear the thumbnail when hovering stops so the exit
            // transition doesn't show the old thumbnail at the
            // playback cursor position (previewPosition switches
            // from hover pos to playback pos when not hovering).
            m_currentThumbnailUrl = QUrl();
            emit thumbnailUrlChanged();
            m_pendingThumbnailUrl = QUrl();
            m_debounceTimer.stop();
            m_urlDebounceTimer.stop();
        }

        emit hoveringChanged();
    }
}

void TimelinePreviewController::setThumbnailInterval(int interval)
{
    if (m_interval != interval)
    {
        m_interval = qMax(1, interval);
        m_generator->setInterval(m_interval);
        emit thumbnailIntervalChanged();
    }
}

void TimelinePreviewController::setMediaLength(double lengthSec)
{
    if (!qFuzzyCompare(m_mediaLength, lengthSec))
    {
        m_mediaLength = lengthSec;
        emit mediaLengthChanged();
    }
}

void TimelinePreviewController::onMediaChanged(input_item_t *inputItem, const QString &mediaHash)
{
    Q_UNUSED(inputItem);

    // If no hash provided, try to compute from current media URL
    QString effectiveHash = mediaHash;
    if (effectiveHash.isEmpty() && m_playerController)
    {
        QUrl url = m_playerController->getUrl();
        effectiveHash = computeHash(url.toString());
    }

    // If the hash is the same as the current media, this is just a pipeline
    // reconfiguration (e.g. fullscreen toggle, codec change) for the same
    // media. Don't clear state or restart pregeneration to avoid flickering
    // the thumbnail while the user is hovering.
    if (!effectiveHash.isEmpty() && effectiveHash == m_currentMediaHash)
        return;

    m_currentMediaHash = effectiveHash;
    m_currentThumbnailUrl = QUrl();
    emit thumbnailUrlChanged();
    m_timestampText.clear();
    emit timestampTextChanged();
    m_chapterTitle.clear();
    emit chapterTitleChanged();
    m_hasChapter = false;
    emit hasChapterChanged();
    m_lastRequestedExactSecond = -1;
    m_pendingThumbnailUrl = QUrl();
    m_urlDebounceTimer.stop();
    m_pregenerationNextIndex = 0;
    m_pregenerationTotal = 0;

    // Pre-generate thumbnails for the new media
    if (m_enabled)
        pregenerateThumbnails();
}

void TimelinePreviewController::onThumbnailReady(const QString &mediaHash, int position, const QImage &image)
{
    if (mediaHash != m_currentMediaHash)
        return;

    // Check if this thumbnail position matches the current hover position
    int currentSeconds = positionToSeconds();

    // Accept a match if positions are close (within 1 second) —
    // this covers both pregenerated thumbnails and precise hover results
    bool matches = (qAbs(position - currentSeconds) <= 1);

    if (matches)
    {
        // Use a unique filename per (mediaHash, exact position) so that
        // thumbnails for different positions never overwrite each other,
        // which was causing QML Image flickering/glitching.
        QString tempPath = m_cache->cacheDirectory() + QDir::separator() +
                          QString("_preview_%1_%2.jpg")
                              .arg(mediaHash)
                              .arg(position, 8, 10, QLatin1Char('0'));

        if (image.save(tempPath, "JPG"))
        {
            // Debounce URL updates to avoid rapid flickering in QML
            m_pendingThumbnailUrl = QUrl::fromLocalFile(tempPath);
            m_urlDebounceTimer.start();
        }
    }

    // Advance progressive pregeneration on each successful thumbnail
    queueNextPregenerationBatch();
}

void TimelinePreviewController::updateChapterInfo()
{
    if (!m_playerController)
        return;

    ChapterListModel *chapters = m_playerController->getChapters();
    if (!chapters || chapters->getCount() == 0)
    {
        m_hasChapter = false;
        m_chapterTitle.clear();
        emit hasChapterChanged();
        emit chapterTitleChanged();
        return;
    }

    // Find which chapter the hover position falls into
    QString chapterName = chapters->getNameAtPosition(static_cast<float>(m_hoverPosition));
    if (!chapterName.isEmpty())
    {
        m_hasChapter = true;
        m_chapterTitle = chapterName;
    }
    else
    {
        m_hasChapter = false;
        m_chapterTitle.clear();
    }

    emit hasChapterChanged();
    emit chapterTitleChanged();
}

void TimelinePreviewController::pregenerateThumbnails()
{
    if (m_currentMediaHash.isEmpty() || m_mediaLength <= 0)
        return;

    int totalThumbnails = static_cast<int>(m_mediaLength) / m_interval;

    // Limit pre-generation to avoid excessive work
    const int MAX_PREGENERATE = 200;
    totalThumbnails = qMin(totalThumbnails, MAX_PREGENERATE);

    m_pregenerationNextIndex = 0;
    m_pregenerationTotal = totalThumbnails;

    // Queue the initial batch — subsequent batches are fed in by
    // queueNextPregenerationBatch() as thumbnails complete.
    queueNextPregenerationBatch();
}

void TimelinePreviewController::queueNextPregenerationBatch()
{
    if (!m_enabled || m_pregenerationNextIndex >= m_pregenerationTotal)
    {
        m_pregenerationNextIndex = 0;
        m_pregenerationTotal = 0;
        return;
    }

    input_item_t *inputItem = m_playerController
        ? m_playerController->getInput() : nullptr;
    if (!inputItem)
    {
        m_pregenerationNextIndex = 0;
        m_pregenerationTotal = 0;
        return;
    }

    // Keep a small number of requests in flight at a time, so the
    // preparser is never flooded on long videos.
    const int MAX_INFLIGHT = 3;
    int queued = 0;
    while (m_pregenerationNextIndex < m_pregenerationTotal && queued < MAX_INFLIGHT)
    {
        int position = m_pregenerationNextIndex * m_interval;
        m_pregenerationNextIndex++;

        if (m_generator->requestThumbnail(inputItem, m_currentMediaHash, position))
            queued++;
    }
}

void TimelinePreviewController::onMediaUrlChanged(const QString &url)
{
    QString hash = computeHash(url);
    onMediaChanged(nullptr, hash);
}

QString TimelinePreviewController::computeHash(const QString &url)
{
    if (url.isEmpty())
        return QString();

    QByteArray data = url.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

int TimelinePreviewController::positionToSeconds() const
{
    if (m_mediaLength <= 0)
        return 0;

    return static_cast<int>(m_hoverPosition * m_mediaLength);
}
