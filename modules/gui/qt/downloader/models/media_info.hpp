/*****************************************************************************
 * media_info.hpp
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_DOWNLOADER_MEDIA_INFO_HPP
#define VLC_DOWNLOADER_MEDIA_INFO_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace vlc {
namespace downloader {

/**
 * @brief Describes a single video format option available for download.
 *
 * Maps to a yt-dlp format entry or equivalent from other providers.
 * Value type — copyable, movable.
 */
struct VideoFormat
{
    std::string id;            /**< Provider-specific format identifier (e.g. "137" for yt-dlp) */
    int height = 0;            /**< Video height in pixels (0 if audio-only) */
    int width = 0;             /**< Video width in pixels */
    std::string codec;         /**< Video codec name (e.g. "avc1", "vp9") */
    std::string extension;     /**< Container extension (e.g. "mp4", "webm") */
    int64_t bitrate = 0;       /**< Average bitrate in bps */
    int64_t filesize = 0;      /**< File size in bytes, 0 if unknown */
    bool hasAudio = false;     /**< Whether this format includes an audio stream */
    bool hasVideo = true;      /**< Whether this format includes a video stream */
};

/**
 * @brief Describes an audio format option available for download or conversion.
 */
struct AudioFormat
{
    std::string id;            /**< Provider-specific format identifier */
    std::string codec;         /**< Audio codec name (e.g. "aac", "opus", "mp3") */
    int bitrate = 0;           /**< Average bitrate in kbps */
    int sampleRate = 0;        /**< Sample rate in Hz */
    std::string extension;     /**< File extension (e.g. "m4a", "webm") */
};

/**
 * @brief Describes a subtitle track available for download.
 */
struct SubtitleTrack
{
    std::string id;            /**< Provider-specific subtitle identifier */
    std::string language;      /**< ISO 639 language code (e.g. "en", "fr") */
    std::string name;          /**< Human-readable name (e.g. "English", "Français") */
    bool isAutomatic = false;  /**< Whether this is an auto-generated caption */
};

/**
 * @brief Describes a single chapter within the media.
 *
 * Chapters are time-ranged markers with a title, similar to DVD chapters
 * or YouTube video chapters.
 */
struct Chapter
{
    std::string title;         /**< Chapter title */
    int64_t startTime = 0;     /**< Start time in seconds from beginning */
    int64_t endTime = 0;       /**< End time in seconds (0 if unknown) */
};

/**
 * @brief Complete metadata about a downloadable media item.
 *
 * This is the central data structure returned by every IMediaProvider::analyze().
 * It normalizes platform-specific metadata into a provider-agnostic representation
 * so the UI and DownloadService never need to know the source platform.
 */
struct MediaInfo
{
    std::string url;              /**< Original URL */
    std::string title;            /**< Media title */
    std::string uploader;         /**< Channel/uploader name */
    std::string description;      /**< Full description text */
    std::string uploadDate;       /**< Upload date string (ISO 8601 or provider format) */
    std::string thumbnailUrl;     /**< URL to the best-quality thumbnail */
    std::string webpageUrl;       /**< Canonical webpage URL */
    std::string extractor;        /**< Provider name (e.g. "youtube", "tiktok") */
    int64_t duration = 0;         /**< Duration in seconds */
    int64_t viewCount = 0;        /**< View count if available */

    /** Available video format options (may be empty for audio-only media) */
    std::vector<VideoFormat> videoFormats;

    /** Available audio format options */
    std::vector<AudioFormat> audioFormats;

    /** Available subtitle tracks */
    std::vector<SubtitleTrack> subtitles;

    /** Chapter markers */
    std::vector<Chapter> chapters;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_MEDIA_INFO_HPP
