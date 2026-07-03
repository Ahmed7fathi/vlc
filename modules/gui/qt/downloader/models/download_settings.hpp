/*****************************************************************************
 * download_settings.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_SETTINGS_HPP
#define VLC_DOWNLOADER_DOWNLOAD_SETTINGS_HPP

#include <string>
#include <cstdint>

namespace vlc {
namespace downloader {

/**
 * @brief User-configurable settings for the downloader.
 *
 * This is a value-type data structure. Defaults are set to sensible values
 * suitable for most users. Integration with VLC's config system will be
 * added in a later phase.
 */
struct DownloadSettings
{
    /** Default directory for downloaded files */
    std::string defaultDownloadPath;

    /** Preferred video height (e.g., 1080 for 1080p). 0 = best available. */
    int preferredVideoHeight = 1080;

    /**
     * Preferred audio codec. Accepted values:
     *   "original" — keep the original stream codec
     *   "aac"      — convert to AAC
     *   "opus"     — convert to Opus
     *   "mp3"      — convert to MP3 (requires FFmpeg)
     *   "flac"     — convert to FLAC (requires FFmpeg)
     *   "wav"      — convert to WAV  (requires FFmpeg)
     */
    std::string preferredAudioCodec = "original";

    /** Preferred subtitle language as ISO 639 code (e.g., "en", "fr"). */
    std::string preferredSubtitleLanguage = "en";

    /** Maximum number of simultaneous downloads. */
    int maxConcurrentDownloads = 1;

    /**
     * Filename template using yt-dlp-style format specifiers:
     *   %(title)s      — video title
     *   %(id)s         — video ID
     *   %(uploader)s   — uploader/channel name
     *   %(height)s     — video height
     *   %(ext)s        — file extension
     */
    std::string filenameTemplate = "%(title)s.%(ext)s";

    /** HTTP/HTTPS proxy URL (empty = no proxy). */
    std::string proxyUrl;

    /** Path to a cookies.txt file (empty = none). */
    std::string cookiesPath;

    /** Whether to embed the thumbnail as cover art. */
    bool embedThumbnail = true;

    /** Whether to embed chapter markers. */
    bool embedChapters = false;

    /** Whether to embed metadata (title, uploader, description, upload date). */
    bool embedMetadata = true;

    /** Whether to embed subtitles into the output file. */
    bool embedSubtitles = false;

    /** Whether to include auto-generated subtitles (e.g., YouTube ASR). */
    bool includeAutoSubtitles = false;

    /** Whether to use SponsorBlock for skipping sponsored segments (future). */
    bool enableSponsorBlock = false;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_SETTINGS_HPP
