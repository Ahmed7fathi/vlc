/*****************************************************************************
 * ffmpeg_processor.cpp
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

#include "ffmpeg_processor.hpp"
#include "../utils/process_runner.hpp"
#include "../utils/cancellation_token.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>

#include <cstring>

namespace vlc {
namespace downloader {

FFmpegProcessor::FFmpegProcessor(vlc_object_t* vlcObj, Operation operation)
    : m_vlcObj(vlcObj)
    , m_operation(operation)
{
}

std::string FFmpegProcessor::name() const
{
    switch (m_operation)
    {
        case Operation::MergeFormats:    return "ffmpeg-merge";
        case Operation::EmbedMetadata:   return "ffmpeg-embed-metadata";
        case Operation::EmbedSubtitles:  return "ffmpeg-embed-subtitles";
        case Operation::ConvertContainer: return "ffmpeg-convert";
    }
    return "ffmpeg";
}

std::string FFmpegProcessor::resolveFfmpegPath() const
{
    if (m_vlcObj && config_FindConfig("ffmpeg-path"))
    {
        char* path = var_InheritString(m_vlcObj, "ffmpeg-path");
        if (path)
        {
            std::string result(path);
            free(path);
            if (!result.empty())
                return result;
        }
    }
    return "ffmpeg";
}

FFmpegProcessor::ArgList FFmpegProcessor::buildArgs(
    const std::string& inputPath,
    const std::string& outputPath,
    const DownloadTask& task) const
{
    ArgList result;
    auto& args = result.args;
    auto& owners = result.owners;

    /* Do NOT include argv[0] here — ProcessRunner::start() prepends
     * opts.executablePath as argv[0]. Adding it here would insert a
     * duplicate that ffmpeg interprets as an input filename argument. */

    /* Common flags */
    args.push_back("-y");            /* Overwrite output files */
    args.push_back("-hide_banner");  /* Reduce noise in stderr */
    args.push_back("-loglevel");
    args.push_back("error");         /* Only show errors */

    switch (m_operation)
    {
        case Operation::MergeFormats:
        {
            args.push_back("-i");
            args.push_back(inputPath.c_str());
            /* Copy all streams without re-encoding */
            args.push_back("-map");
            args.push_back("0");
            args.push_back("-c");
            args.push_back("copy");
            break;
        }

        case Operation::EmbedMetadata:
        {
            args.push_back("-i");
            args.push_back(inputPath.c_str());

            /* Embed title if available */
            if (task.mediaInfo() && !task.mediaInfo()->title.empty())
            {
                args.push_back("-metadata");
                owners.emplace_back("title=" + task.mediaInfo()->title);
                args.push_back(owners.back().c_str());

                if (!task.mediaInfo()->uploader.empty())
                {
                    args.push_back("-metadata");
                    owners.emplace_back("artist=" + task.mediaInfo()->uploader);
                    args.push_back(owners.back().c_str());
                }
            }

            args.push_back("-map");
            args.push_back("0");
            args.push_back("-c");
            args.push_back("copy");
            break;
        }

        case Operation::EmbedSubtitles:
        {
            args.push_back("-i");
            args.push_back(inputPath.c_str());

            /* Add each subtitle file as an additional input */
            auto subtitles = task.selectedSubtitles();
            for (size_t i = 0; i < subtitles.size(); ++i)
            {
                if (subtitles[i] && !subtitles[i]->language.empty())
                {
                    /* Subtitle tracks are identified by language; yt-dlp handles download */
                    args.push_back("-i");
                    args.push_back(subtitles[i]->language.c_str());
                }
            }

            /* Map all streams from input and sub streams from additional inputs */
            args.push_back("-map");
            args.push_back("0");
            for (size_t i = 0; i < subtitles.size(); ++i)
            {
                if (subtitles[i] && !subtitles[i]->language.empty())
                {
                    args.push_back("-map");
                    owners.emplace_back(std::to_string(i + 1) + ":0");
                    args.push_back(owners.back().c_str());
                }
            }

            args.push_back("-c");
            args.push_back("copy");
            args.push_back("-disposition:s:0");
            args.push_back("default");
            break;
        }

        case Operation::ConvertContainer:
        {
            args.push_back("-i");
            args.push_back(inputPath.c_str());
            args.push_back("-map");
            args.push_back("0");
            args.push_back("-c");
            args.push_back("copy");
            break;
        }
    }

    args.push_back(outputPath.c_str());
    args.push_back(nullptr);

    return result;
}

IMediaProcessor::Result FFmpegProcessor::process(
    const std::string& inputPath,
    const std::string& outputPath,
    const DownloadTask& task,
    CancellationToken* token)
{
    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "FFmpegProcessor(%s): %s -> %s",
                name().c_str(), inputPath.c_str(), outputPath.c_str());

    /* Build args — the ArgList struct keeps all backing strings alive */
    auto argList = buildArgs(inputPath, outputPath, task);

    /* Build options for synchronous ProcessRunner::run() */
    std::string ffmpegPath = resolveFfmpegPath(); /* keep alive for opts */
    ProcessRunner::Options opts;
    opts.executablePath = ffmpegPath.c_str();
    opts.args = argList.args;  /* pointers valid as long as argList lives */
    opts.captureStdout = false;
    opts.captureStderr = true;
    opts.logger = m_vlcObj;

    auto processResult = ProcessRunner::run(opts, token);

    if (processResult.succeeded)
    {
        if (m_vlcObj)
            msg_Dbg(m_vlcObj, "FFmpegProcessor(%s): completed successfully", name().c_str());

        return Result{true, outputPath, ""};
    }

    std::string error = "ffmpeg " + name() + " failed: " + processResult.stdErr;
    if (processResult.exitStatus != -1)
        error += " (exit code " + std::to_string(processResult.exitStatus) + ")";

    if (m_vlcObj)
        msg_Err(m_vlcObj, "FFmpegProcessor(%s): %s", name().c_str(), error.c_str());

    return Result{false, "", error};
}

} // namespace downloader
} // namespace vlc
