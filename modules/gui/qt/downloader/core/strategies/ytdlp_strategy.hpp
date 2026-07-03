/*****************************************************************************
 * ytdlp_strategy.hpp
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

#ifndef VLC_DOWNLOADER_YTDLP_STRATEGY_HPP
#define VLC_DOWNLOADER_YTDLP_STRATEGY_HPP

#include "../interfaces/i_download_strategy.hpp"

#include <vector>
#include <string>

struct vlc_object_t;

namespace vlc {
namespace downloader {

/**
 * @brief Download strategy using yt-dlp for media downloads.
 *
 * Spawns yt-dlp with format selection based on the DownloadTask's
 * user selections. Monitors stderr output for real-time progress
 * (percentage, speed, ETA) using YtdlpProgressParser.
 *
 * Supports:
 *   - Video download with format selection
 *   - Audio-only download
 *   - Subtitle download
 *   - Cancellation via CancellationToken
 *   - Multiple simultaneous downloads (separate instances)
 */
class YtdlpStrategy : public IDownloadStrategy
{
public:
    /**
     * @brief Create a YtdlpStrategy.
     *
     * @param vlcObj      VLC object for config access and logging.
     * @param ytdlpPath   Path to yt-dlp executable (resolved from VLC config).
     */
    explicit YtdlpStrategy(vlc_object_t* vlcObj, const std::string& ytdlpPath);
    ~YtdlpStrategy() override = default;

    /** Non-copyable, movable */
    YtdlpStrategy(const YtdlpStrategy&) = delete;
    YtdlpStrategy& operator=(const YtdlpStrategy&) = delete;
    YtdlpStrategy(YtdlpStrategy&&) = default;
    YtdlpStrategy& operator=(YtdlpStrategy&&) = default;

    std::string name() const override { return "yt-dlp"; }

    Result execute(const DownloadTask& task,
                   const std::string& outputPath,
                   ProgressCallback onProgress,
                   CancellationToken* token) override;

private:
    /**
     * @brief A built argument list with backing string storage.
     *
     * The `args` vector holds `const char*` pointers into memory owned by
     * `owners`. As long as this struct is alive, all pointers are valid.
     */
    struct ArgList
    {
        std::vector<const char*> args;
        std::vector<std::string> owners;  /**< Keeps string data alive */
    };

    /** Build yt-dlp argument list from task selections. */
    ArgList buildArgs(const DownloadTask& task,
                      const std::string& outputPath) const;

    /** Extract error message from yt-dlp stderr output. */
    static std::string extractError(const std::string& stderr);

    vlc_object_t* m_vlcObj;
    std::string m_ytdlpPath;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_YTDLP_STRATEGY_HPP
