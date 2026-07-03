/*****************************************************************************
 * processing_pipeline.hpp
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

#ifndef VLC_DOWNLOADER_PROCESSING_PIPELINE_HPP
#define VLC_DOWNLOADER_PROCESSING_PIPELINE_HPP

#include "../interfaces/i_media_processor.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vlc {
namespace downloader {

class CancellationToken;

/**
 * @brief Composable pipeline of post-processing steps.
 *
 * Each step in the pipeline is an IMediaProcessor that processes the
 * downloaded file in sequence. The output of one step becomes the input
 * of the next. Temp files are managed between steps.
 *
 * Usage:
 * @code
 *   ProcessingPipeline pipeline;
 *   pipeline.addStep(std::make_unique<FFmpegProcessor>(..., MergeFormats));
 *   pipeline.addStep(std::make_unique<FFmpegProcessor>(..., EmbedMetadata));
 *   auto result = pipeline.process(inputPath, finalPath, task, token);
 * @endcode
 *
 * If any step fails, the pipeline stops and returns the error.
 * Temp files from intermediate steps are cleaned up automatically.
 */
class ProcessingPipeline
{
public:
    ProcessingPipeline() = default;
    ~ProcessingPipeline() = default;

    /** Non-copyable, movable */
    ProcessingPipeline(const ProcessingPipeline&) = delete;
    ProcessingPipeline& operator=(const ProcessingPipeline&) = delete;
    ProcessingPipeline(ProcessingPipeline&&) = default;
    ProcessingPipeline& operator=(ProcessingPipeline&&) = default;

    /**
     * @brief Add a processing step to the pipeline.
     * Steps are executed in the order they are added.
     */
    void addStep(std::unique_ptr<IMediaProcessor> processor)
    {
        m_steps.push_back(std::move(processor));
    }

    /** @brief Number of steps in the pipeline. */
    size_t stepCount() const { return m_steps.size(); }

    /** @brief Whether the pipeline has no steps. */
    bool empty() const { return m_steps.empty(); }

    /**
     * @brief Execute all processing steps in sequence.
     *
     * @param inputPath   Path to the downloaded file.
     * @param outputPath  Desired final output path.
     * @param task        The download task for context.
     * @param token       Cancellation token.
     * @return Result from the last step, or the first failure.
     */
    IMediaProcessor::Result process(const std::string& inputPath,
                                    const std::string& outputPath,
                                    const DownloadTask& task,
                                    CancellationToken* token);

private:
    std::vector<std::unique_ptr<IMediaProcessor>> m_steps;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_PROCESSING_PIPELINE_HPP
