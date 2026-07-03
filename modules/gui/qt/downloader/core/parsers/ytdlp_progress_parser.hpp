/*****************************************************************************
 * ytdlp_progress_parser.hpp
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

#ifndef VLC_DOWNLOADER_YTDLP_PROGRESS_PARSER_HPP
#define VLC_DOWNLOADER_YTDLP_PROGRESS_PARSER_HPP

#include <string>
#include <cstdint>
#include <optional>

namespace vlc {
namespace downloader {

/**
 * @brief Structured progress data extracted from yt-dlp stderr output.
 */
struct YtdlpProgressData
{
    int percent = 0;          /**< Download progress (0–100) */
    double speed = 0.0;       /**< Download speed in bytes/sec */
    int64_t eta = 0;          /**< Estimated time remaining in seconds */
    int64_t downloadedBytes = 0; /**< Bytes downloaded so far */
    int64_t totalBytes = 0;   /**< Total bytes to download (0 if unknown) */
    std::string currentFile;  /**< Currently processed file */
    bool isComplete = false;  /**< Whether this line indicates completion */
};

/**
 * @brief Parses yt-dlp's stderr progress lines into structured data.
 *
 * yt-dlp outputs progress lines in various formats:
 * @code
 *   [download] Destination: video.mp4
 *   [download]   0.0% of    1.25MiB at    0.00MiB/s ETA 00:00
 *   [download]  10.2% of    1.25MiB at    1.23MiB/s ETA 00:05
 *   [download] 100.0% of    1.25MiB at    2.34MiB/s ETA 00:00
 *   [download] 100% of 1.25MiB in 00:00
 *   [ffmpeg] Merging formats into "output.mp4"
 * @endcode
 *
 * This parser extracts percentage, speed, ETA, and file size from these lines.
 */
class YtdlpProgressParser
{
public:
    /**
     * @brief Parse a single line of yt-dlp stderr output.
     *
     * @param line One line of stderr output (without trailing newline).
     * @return Populated YtdlpProgressData if the line contains progress info,
     *         or std::nullopt if the line is informational (e.g., "Destination: ...").
     */
    static std::optional<YtdlpProgressData> parseLine(const std::string& line);

    /**
     * @brief Check if a line indicates a file destination.
     * e.g., "[download] Destination: /path/to/file.mp4"
     *
     * @param line  The stderr line.
     * @param path  [out] Receives the destination path if found.
     * @return true if the line is a destination line.
     */
    static bool parseDestination(const std::string& line, std::string& path);
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_YTDLP_PROGRESS_PARSER_HPP
