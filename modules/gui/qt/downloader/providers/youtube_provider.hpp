/*****************************************************************************
 * youtube_provider.hpp
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

#ifndef VLC_DOWNLOADER_YOUTUBE_PROVIDER_HPP
#define VLC_DOWNLOADER_YOUTUBE_PROVIDER_HPP

#include "../core/interfaces/i_media_provider.hpp"
#include "../core/utils/cancellation_token.hpp"

#include <vlc_common.h>

#include <memory>
#include <string>

namespace vlc {
namespace downloader {

/**
 * @brief IMediaProvider implementation for YouTube.
 *
 * Uses yt-dlp to extract metadata from YouTube URLs.
 * Analysis is performed by spawning `yt-dlp --dump-single-json -- <url>`
 * and parsing the JSON output into a MediaInfo object.
 *
 * The yt-dlp executable path is resolved from VLC's "ytdl-path" configuration
 * variable (default: "yt-dlp" on PATH).
 *
 * URL patterns matched:
 *   - youtube.com/watch?v=...
 *   - youtu.be/...
 *   - m.youtube.com/watch?v=...
 *   - www.youtube.com/...
 *   - music.youtube.com/...
 *   - youtube.com/shorts/...
 *   - youtube.com/embed/...
 */
class YoutubeProvider : public IMediaProvider
{
public:
    /**
     * @brief Create a YoutubeProvider.
     *
     * @param vlcObj  VLC object for config access and logging.
     *                Must remain valid for the provider's lifetime.
     */
    explicit YoutubeProvider(vlc_object_t* vlcObj);
    ~YoutubeProvider() override;

    /** Non-copyable, movable */
    YoutubeProvider(const YoutubeProvider&) = delete;
    YoutubeProvider& operator=(const YoutubeProvider&) = delete;
    YoutubeProvider(YoutubeProvider&&) = delete;
    YoutubeProvider& operator=(YoutubeProvider&&) = delete;

    // ── IMediaProvider interface ─────────────────────────────────────────

    std::string name() const override { return "YouTube"; }

    bool canHandle(const std::string& url) const override;

    void analyze(std::shared_ptr<DownloadTask> task,
                 AnalyzeSuccessCallback onSuccess,
                 AnalyzeErrorCallback onError) override;

    void cancel() override;

private:
    /** Resolve the yt-dlp executable path from VLC config. */
    std::string resolveYtdlpPath() const;

    /** Shared cancellation token for interrupting in-flight analysis */
    std::shared_ptr<CancellationToken> m_cancelToken;

    vlc_object_t* m_vlcObj;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_YOUTUBE_PROVIDER_HPP
