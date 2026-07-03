/*****************************************************************************
 * i_media_provider.hpp
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

#ifndef VLC_DOWNLOADER_I_MEDIA_PROVIDER_HPP
#define VLC_DOWNLOADER_I_MEDIA_PROVIDER_HPP

#include "../../models/media_info.hpp"
#include "../../models/download_task.hpp"

#include <memory>
#include <string>
#include <functional>

namespace vlc {
namespace downloader {

/**
 * @brief Abstract interface for platform-specific media analysis.
 *
 * Every supported platform (YouTube, TikTok, Instagram, etc.) implements
 * this interface. Providers are responsible for ONE thing only: analyzing
 * a URL and returning normalized MediaInfo.
 *
 * Providers do NOT perform downloads. The DownloadEngine handles all
 * download execution, post-processing, and FFmpeg operations.
 *
 * To add a new platform:
 *   1. Implement this interface
 *   2. Register it with ProviderRegistry
 *   3. No UI changes needed — the UI works with MediaInfo objects
 */
class IMediaProvider
{
public:
    virtual ~IMediaProvider() = default;

    /** @brief Human-readable provider name (e.g., "YouTube", "TikTok"). */
    virtual std::string name() const = 0;

    /**
     * @brief Check whether this provider can handle the given URL.
     *
     * Typically implemented with URL pattern matching (regex or prefix check).
     * Should be fast — called for every registered provider during URL lookup.
     */
    virtual bool canHandle(const std::string& url) const = 0;

    // ── Callback types ───────────────────────────────────────────────────

    /** Called on successful analysis. */
    using AnalyzeSuccessCallback = std::function<void(std::shared_ptr<DownloadTask>,
                                                      std::unique_ptr<MediaInfo>)>;

    /** Called on analysis failure. */
    using AnalyzeErrorCallback = std::function<void(std::shared_ptr<DownloadTask>,
                                                    const std::string& errorMessage)>;

    /**
     * @brief Analyze a URL and extract metadata.
     *
     * This is the central method of the provider interface.
     * The provider must:
     *   1. Validate the URL format
     *   2. Fetch metadata (e.g., via yt-dlp --dump-json for YouTube)
     *   3. Parse into a MediaInfo object
     *   4. Call onSuccess or onError
     *
     * Providers should attempt to process cancellation via cancel() between
     * heavy operations.
     *
     * @param task      The download task (url is accessible via task->url()).
     * @param onSuccess Callback with the populated MediaInfo.
     * @param onError   Callback with an error description.
     */
    virtual void analyze(std::shared_ptr<DownloadTask> task,
                         AnalyzeSuccessCallback onSuccess,
                         AnalyzeErrorCallback onError) = 0;

    /**
     * @brief Cancel any ongoing analysis.
     *
     * Called when the user cancels before analysis completes.
     * Implementations should stop any network requests or subprocesses.
     */
    virtual void cancel() = 0;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_I_MEDIA_PROVIDER_HPP
