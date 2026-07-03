/*****************************************************************************
 * i_media_processor.hpp
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

#ifndef VLC_DOWNLOADER_I_MEDIA_PROCESSOR_HPP
#define VLC_DOWNLOADER_I_MEDIA_PROCESSOR_HPP

#include "../../models/download_task.hpp"

#include <memory>
#include <string>

namespace vlc {
namespace downloader {

class CancellationToken;

/**
 * @brief Abstract post-processing step for downloaded media.
 *
 * After a file is downloaded (e.g., via yt-dlp), processing steps may need
 * to run:
 *   - Merging separate audio/video streams
 *   - Converting to a target container format
 *   - Embedding subtitles, chapters, or metadata
 *   - Compressing or resizing
 *
 * Each processor implements this interface and can be composed into
 * a ProcessingPipeline for sequential execution.
 */
class IMediaProcessor
{
public:
    virtual ~IMediaProcessor() = default;

    /** @brief Human-readable processor name (e.g., "ffmpeg-merge", "metadata-embed"). */
    virtual std::string name() const = 0;

    /**
     * @brief Result of a processing operation.
     */
    struct Result
    {
        bool succeeded = false;
        std::string outputPath;  /**< Path to the processed output file */
        std::string errorMessage;
    };

    /**
     * @brief Process a media file.
     *
     * @param inputPath   Path to the downloaded/input file.
     * @param outputPath  Desired output path (may be same as input for in-place processing).
     * @param task        The download task (for context: selections, options, media info).
     * @param token       Cancellation token for aborting the operation.
     * @return Result indicating success or failure.
     */
    virtual Result process(const std::string& inputPath,
                           const std::string& outputPath,
                           const DownloadTask& task,
                           CancellationToken* token) = 0;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_I_MEDIA_PROCESSOR_HPP
