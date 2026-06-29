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

#ifndef THUMBNAIL_CACHE_HPP
#define THUMBNAIL_CACHE_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QString>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QDir>

/**
 * @brief ThumbnailCache manages disk-based caching of timeline thumbnails.
 *
 * This class provides a persistent cache for generated thumbnails, keyed by
 * media identifier and timestamp position. It uses LRU (Least Recently Used)
 * eviction when the cache exceeds the configured size limit.
 *
 * The cache is stored inside the user's VLC cache directory under a
 * "timeline-thumbnails" subdirectory.
 */
class ThumbnailCache : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a ThumbnailCache
     * @param parent Parent QObject
     */
    explicit ThumbnailCache(QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ThumbnailCache() override;

    /**
     * @brief Get the cache directory path
     * @return Absolute path to the cache directory
     */
    QString cacheDirectory() const;

    /**
     * @brief Set the cache directory path
     * @param path Absolute path to the cache directory
     */
    void setCacheDirectory(const QString &path);

    /**
     * @brief Get the maximum cache size in megabytes
     * @return Maximum cache size (MB)
     */
    int maxCacheSize() const;

    /**
     * @brief Set the maximum cache size in megabytes
     * @param sizeMB Maximum cache size (MB)
     */
    void setMaxCacheSize(int sizeMB);

    /**
     * @brief Get the thumbnail interval in seconds
     * @return Interval in seconds
     */
    int interval() const;

    /**
     * @brief Set the thumbnail interval in seconds
     * @param sec Interval in seconds
     */
    void setInterval(int sec);

    /**
     * @brief Get the thumbnail width
     * @return Width in pixels
     */
    int thumbnailWidth() const;

    /**
     * @brief Set the thumbnail width
     * @param width Width in pixels
     */
    void setThumbnailWidth(int width);

    /**
     * @brief Check if a thumbnail exists in the cache for the given media and position
     * @param mediaHash Unique hash for the media (e.g. from input_item_t)
     * @param position Position in seconds
     * @return true if cached thumbnail exists
     */
    bool hasThumbnail(const QString &mediaHash, int position) const;

    /**
     * @brief Retrieve a cached thumbnail
     * @param mediaHash Unique hash for the media
     * @param position Position in seconds
     * @return QImage of the thumbnail, or null QImage if not found
     */
    QImage getThumbnail(const QString &mediaHash, int position) const;

    /**
     * @brief Store a thumbnail in the cache
     * @param mediaHash Unique hash for the media
     * @param position Position in seconds
     * @param image The thumbnail image to store
     */
    void putThumbnail(const QString &mediaHash, int position, const QImage &image);

    /**
     * @brief Clear all cached thumbnails for the given media
     * @param mediaHash Unique hash for the media
     */
    void clearMediaCache(const QString &mediaHash);

    /**
     * @brief Clear the entire thumbnail cache
     */
    void clearAll();

    /**
     * @brief Get the cache directory path used for timeline thumbnails
     * @return The cache directory path
     */
    static QString defaultCacheDirectory();

signals:
    /**
     * @brief Emitted when the cache is cleared
     */
    void cacheCleared();

private:
    /**
     * @brief Generate a filename for a cached thumbnail
     * @param mediaHash Unique hash for the media
     * @param position Position in seconds
     * @return Filename suitable for the cache directory
     */
    QString cacheFilename(const QString &mediaHash, int position) const;

    /**
     * @brief Enforce the cache size limit by removing least recently used entries
     */
    void enforceCacheLimit();

    /**
     * @brief Compute the current cache directory size in bytes
     * @return Size in bytes
     */
    qint64 computeCacheSize() const;

    QString m_cacheDir;
    int m_maxCacheSizeMB = 500;   // Default 500 MB
    int m_interval = 5;           // Default 5 seconds
    int m_thumbnailWidth = 320;   // Default 320px wide

    mutable QMutex m_mutex;
    // In-memory LRU tracking: mediaHash -> list of positions accessed
    mutable QHash<QString, QList<int>> m_lruTracker;
};

#endif // THUMBNAIL_CACHE_HPP
