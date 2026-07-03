/*****************************************************************************
 * ffmpeg_processor.hpp
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

#ifndef VLC_DOWNLOADER_FFMPEG_PROCESSOR_HPP
#define VLC_DOWNLOADER_FFMPEG_PROCESSOR_HPP

#include "../interfaces/i_media_processor.hpp"

struct vlc_object_t;

namespace vlc {
namespace downloader {

/**
 * @brief Post-processing via ffmpeg.
 *
 * Handles the following operations by spawning ffmpeg as a subprocess:
 *
 *  - Format merge: Combines separate audio+video streams (yt-dlp's default
 *    for many sites) into a single output file using stream copy.
 *
 *  - Metadata embedding: Embeds title, description, uploader, and thumbnail
 *    into the output file's metadata fields.
 *
 *  - Subtitle embedding: Muxes external subtitle files into the container.
 *
 * The ffmpeg path is resolved from VLC config ("ffmpeg-path" variable)
 * with a fallback to "ffmpeg" on PATH.
 */
class FFmpegProcessor : public IMediaProcessor
{
public:
    /**
     * @brief Type of processing to perform.
     */
    enum class Operation
    {
        MergeFormats,     /**< Merge separate audio+video streams */
        EmbedMetadata,    /**< Embed title/description/thumbnail */
        EmbedSubtitles,   /**< Mux subtitles into container */
        ConvertContainer  /**< Convert to specified container format */
    };

    /**
     * @brief Create an FFmpegProcessor.
     *
     * @param vlcObj     VLC object for config access and logging.
     * @param operation  The type of post-processing operation.
     */
    explicit FFmpegProcessor(vlc_object_t* vlcObj, Operation operation);
    ~FFmpegProcessor() override = default;

    /** Non-copyable, movable */
    FFmpegProcessor(const FFmpegProcessor&) = delete;
    FFmpegProcessor& operator=(const FFmpegProcessor&) = delete;
    FFmpegProcessor(FFmpegProcessor&&) = default;
    FFmpegProcessor& operator=(FFmpegProcessor&&) = default;

    std::string name() const override;

    Result process(const std::string& inputPath,
                   const std::string& outputPath,
                   const DownloadTask& task,
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

    /** Build ffmpeg arguments for the configured operation. */
    ArgList buildArgs(const std::string& inputPath,
                      const std::string& outputPath,
                      const DownloadTask& task) const;

    /** Resolve ffmpeg executable path from VLC config. */
    std::string resolveFfmpegPath() const;

    vlc_object_t* m_vlcObj;
    Operation m_operation;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_FFMPEG_PROCESSOR_HPP
