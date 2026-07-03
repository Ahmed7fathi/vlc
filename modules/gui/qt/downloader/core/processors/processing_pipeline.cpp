/*****************************************************************************
 * processing_pipeline.cpp
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

#include "processing_pipeline.hpp"
#include "../utils/cancellation_token.hpp"
#include "../utils/temp_file_manager.hpp"

#include <vlc_common.h>
#include <vlc_fs.h>

namespace vlc {
namespace downloader {

IMediaProcessor::Result ProcessingPipeline::process(
    const std::string& inputPath,
    const std::string& outputPath,
    const DownloadTask& task,
    CancellationToken* token)
{
    if (m_steps.empty())
    {
        /* No processing needed — pass through */
        return IMediaProcessor::Result{true, inputPath, ""};
    }

    TempFileManager tempMgr;
    std::string currentInput = inputPath;

    for (size_t i = 0; i < m_steps.size(); ++i)
    {
        if (token && token->isCancelled())
            return IMediaProcessor::Result{false, "",
                "Processing cancelled at step " + std::to_string(i)};

        const bool isLastStep = (i == m_steps.size() - 1);
        const std::string stepOutput = isLastStep
            ? outputPath
            : tempMgr.createTempFile("step_" + std::to_string(i), ".tmp");

        if (stepOutput.empty() && !isLastStep)
        {
            return IMediaProcessor::Result{false, "",
                "Failed to create temp file for processing step " + std::to_string(i)};
        }

        auto result = m_steps[i]->process(
            currentInput, stepOutput, task, token);

        if (!result.succeeded)
            return result;

        /* Remove previous intermediate file (but not the original input) */
        if (i > 0 && currentInput != inputPath)
        {
            tempMgr.releaseFile(currentInput); /* stop tracking */
            vlc_unlink(currentInput.c_str());   /* delete from disk */
        }

        currentInput = stepOutput;
    }

    return IMediaProcessor::Result{true, currentInput, ""};
}

} // namespace downloader
} // namespace vlc
