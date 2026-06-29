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

#include "thumbnail_cache.hpp"
#include "qt.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QCryptographicHash>
#include <QDateTime>
#include <QStorageInfo>
#include <QStandardPaths>

#include <vlc_configuration.h>

ThumbnailCache::ThumbnailCache(QObject *parent)
    : QObject(parent)
{
    m_cacheDir = defaultCacheDirectory();

    // Ensure cache directory exists
    QDir dir(m_cacheDir);
    if (!dir.exists())
        dir.mkpath(".");

    // Load settings
    m_maxCacheSizeMB = config_GetInt("timeline-thumbnail-cache-size");
    if (m_maxCacheSizeMB <= 0)
        m_maxCacheSizeMB = 500;

    m_interval = config_GetInt("timeline-thumbnail-interval");
    if (m_interval <= 0)
        m_interval = 5;

    m_thumbnailWidth = config_GetInt("timeline-thumbnail-width");
    if (m_thumbnailWidth <= 0)
        m_thumbnailWidth = 320;

    // Apply cache limit on startup
    enforceCacheLimit();
}

ThumbnailCache::~ThumbnailCache()
{
}

QString ThumbnailCache::cacheDirectory() const
{
    return m_cacheDir;
}

void ThumbnailCache::setCacheDirectory(const QString &path)
{
    QMutexLocker lock(&m_mutex);
    if (m_cacheDir != path)
    {
        m_cacheDir = path;
        QDir dir(m_cacheDir);
        if (!dir.exists())
            dir.mkpath(".");
    }
}

int ThumbnailCache::maxCacheSize() const
{
    return m_maxCacheSizeMB;
}

void ThumbnailCache::setMaxCacheSize(int sizeMB)
{
    QMutexLocker lock(&m_mutex);
    m_maxCacheSizeMB = sizeMB;
    config_PutInt("timeline-thumbnail-cache-size", sizeMB);
    enforceCacheLimit();
}

int ThumbnailCache::interval() const
{
    return m_interval;
}

void ThumbnailCache::setInterval(int sec)
{
    m_interval = sec;
    config_PutInt("timeline-thumbnail-interval", sec);
}

int ThumbnailCache::thumbnailWidth() const
{
    return m_thumbnailWidth;
}

void ThumbnailCache::setThumbnailWidth(int width)
{
    m_thumbnailWidth = width;
    config_PutInt("timeline-thumbnail-width", width);
}

bool ThumbnailCache::hasThumbnail(const QString &mediaHash, int position) const
{
    QMutexLocker lock(&m_mutex);
    QString filePath = m_cacheDir + QDir::separator() + cacheFilename(mediaHash, position);
    return QFile::exists(filePath);
}

QImage ThumbnailCache::getThumbnail(const QString &mediaHash, int position) const
{
    QMutexLocker lock(&m_mutex);
    QString filePath = m_cacheDir + QDir::separator() + cacheFilename(mediaHash, position);

    if (!QFile::exists(filePath))
        return QImage();

    // Update LRU tracker
    if (!m_lruTracker[mediaHash].contains(position))
    {
        m_lruTracker[mediaHash].append(position);
    }
    else
    {
        m_lruTracker[mediaHash].removeAll(position);
        m_lruTracker[mediaHash].append(position);
    }

    QImageReader reader(filePath);
    return reader.read();
}

void ThumbnailCache::putThumbnail(const QString &mediaHash, int position, const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    QString filePath = m_cacheDir + QDir::separator() + cacheFilename(mediaHash, position);

    QImageWriter writer(filePath, "jpg");
    writer.setQuality(85);
    if (!writer.write(image))
    {
        return;
    }

    // Update LRU tracker
    m_lruTracker[mediaHash].removeAll(position);
    m_lruTracker[mediaHash].append(position);

    // Check cache size
    enforceCacheLimit();
}

void ThumbnailCache::clearMediaCache(const QString &mediaHash)
{
    QMutexLocker lock(&m_mutex);
    QDir dir(m_cacheDir);
    QStringList filters;
    filters << mediaHash + "_*.jpg";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    for (const QString &file : files)
    {
        dir.remove(file);
    }

    m_lruTracker.remove(mediaHash);
}

void ThumbnailCache::clearAll()
{
    QMutexLocker lock(&m_mutex);
    QDir dir(m_cacheDir);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    for (const QString &file : files)
    {
        dir.remove(file);
    }

    m_lruTracker.clear();
    emit cacheCleared();
}

QString ThumbnailCache::defaultCacheDirectory()
{
    char *userDir = config_GetUserDir(VLC_CACHE_DIR);
    if (!userDir)
        return QString();

    QString cacheDir = qfu(userDir);
    free(userDir);

    return cacheDir + QDir::separator() + "timeline-thumbnails";
}

QString ThumbnailCache::cacheFilename(const QString &mediaHash, int position) const
{
    return QString("%1_%2.jpg").arg(mediaHash).arg(position, 8, 10, QLatin1Char('0'));
}

void ThumbnailCache::enforceCacheLimit()
{
    if (m_maxCacheSizeMB <= 0)
        return;

    qint64 maxBytes = static_cast<qint64>(m_maxCacheSizeMB) * 1024 * 1024;
    qint64 currentSize = computeCacheSize();

    if (currentSize <= maxBytes)
        return;

    // Need to evict: collect all cache files with their last access time
    QDir dir(m_cacheDir);
    // Sort by modification time (AccessDate was removed in Qt6, and file access
    // times are unreliable on many modern filesystems due to noatime/relatime)
    QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Time | QDir::Reversed);

    for (const QFileInfo &info : entries)
    {
        if (currentSize <= maxBytes)
            break;

        qint64 fileSize = info.size();
        if (dir.remove(info.fileName()))
        {
            currentSize -= fileSize;

            // Remove from LRU tracker
            QString fileName = info.completeBaseName();
            // Format: mediaHash_position
            int underscoreIdx = fileName.lastIndexOf('_');
            if (underscoreIdx > 0)
            {
                QString hash = fileName.left(underscoreIdx);
                int pos = fileName.mid(underscoreIdx + 1).toInt();
                if (m_lruTracker.contains(hash))
                {
                    m_lruTracker[hash].removeAll(pos);
                    if (m_lruTracker[hash].isEmpty())
                        m_lruTracker.remove(hash);
                }
            }
        }
    }
}

qint64 ThumbnailCache::computeCacheSize() const
{
    qint64 size = 0;
    QDir dir(m_cacheDir);
    QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo &info : entries)
    {
        size += info.size();
    }

    return size;
}
