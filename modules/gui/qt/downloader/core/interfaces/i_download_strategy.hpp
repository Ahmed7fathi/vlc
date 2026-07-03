/*****************************************************************************
 * i_download_strategy.hpp
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

#ifndef VLC_DOWNLOADER_I_DOWNLOAD_STRATEGY_HPP
#define VLC_DOWNLOADER_I_DOWNLOAD_STRATEGY_HPP

#include "../../models/download_task.hpp"
#include "../../core/utils/cancellation_token.hpp"

#include <memory>
#include <string>
#include <functional>

namespace vlc {
namespace downloader {

/**
 * @brief Abstract interface for downloading media from a provider.
 *
 * Different providers may require different download strategies:
 *   - YouTube / general sites: yt-dlp
 *   - Direct HTTP/HTTPS URLs: direct download
 *   - HLS/m3u8 streams: ffmpeg download
 *
 * Each strategy is responsible for executing the download,
 * reporting progress, and handling cancellation.
 */
class IDownloadStrategy
{
public:
    virtual ~IDownloadStrategy() = default;

    /** @brief Human-readable strategy name (e.g., "yt-dlp", "direct"). */
    virtual std::string name() const = 0;

    /**
     * @brief Result of a download operation.
     */
    struct Result
    {
        bool succeeded = false;
        std::string outputPath;  /**< Path to the downloaded file */
        std::string errorMessage;
    };

    /** @brief Progress callback: (percent, speedBytes, etaSecs, downloaded, total) */
    using ProgressCallback = std::function<void(int, double, int64_t, int64_t, int64_t)>;

    /**
     * @brief Execute a download.
     *
     * @param task       The download task with user selections.
     * @param outputPath Where to save the downloaded file.
     * @param onProgress Called periodically with progress updates.
     * @param token      Cancellation token for aborting the download.
     * @return Result indicating success or failure.
     */
    virtual Result execute(const DownloadTask& task,
                           const std::string& outputPath,
                           ProgressCallback onProgress,
                           CancellationToken* token) = 0;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_I_DOWNLOAD_STRATEGY_HPP
