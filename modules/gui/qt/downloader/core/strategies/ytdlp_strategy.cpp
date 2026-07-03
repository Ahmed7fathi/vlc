/*****************************************************************************
 * ytdlp_strategy.cpp
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

#include "ytdlp_strategy.hpp"
#include "../utils/process_runner.hpp"
#include "../utils/cancellation_token.hpp"
#include "../parsers/ytdlp_progress_parser.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>

#include <sstream>
#include <cstring>

namespace vlc {
namespace downloader {

YtdlpStrategy::YtdlpStrategy(vlc_object_t* vlcObj, const std::string& ytdlpPath)
    : m_vlcObj(vlcObj)
    , m_ytdlpPath(ytdlpPath)
{
}

YtdlpStrategy::ArgList YtdlpStrategy::buildArgs(
    const DownloadTask& task,
    const std::string& outputPath) const
{
    ArgList result;
    auto& args = result.args;
    auto& owners = result.owners;

    /* Do NOT include argv[0] here — ProcessRunner::start() prepends
     * opts.executablePath as argv[0]. Adding it here would insert a
     * duplicate that yt-dlp interprets as a URL argument. */

    /* Always download the best format as base */
    if (task.audioOnly())
    {
        /* Best audio-only format */
        args.push_back("-f");
        if (task.selectedAudioFormat())
        {
            /* Use the format code from the selected audio format */
            args.push_back(task.selectedAudioFormat()->id.c_str());
        }
        else
        {
            args.push_back("bestaudio/best");
        }
        args.push_back("--extract-audio");
        args.push_back("--audio-format");
        owners.emplace_back(task.selectedAudioFormat()
            ? task.selectedAudioFormat()->extension : "m4a");
        args.push_back(owners.back().c_str());
    }
    else
    {
        /* Best video+audio (merge if separate) */
        args.push_back("-f");
        if (task.selectedVideoFormat())
        {
            /* Use the selected format's ID for quality restriction.
             * Append +bestaudio/best to ensure we get audio even if the
             * selected format is video-only (common for YouTube DASH). */
            std::string formatExpr = task.selectedVideoFormat()->id + "+bestaudio/best";
            owners.push_back(formatExpr);
            args.push_back(owners.back().c_str());
        }
        else
        {
            args.push_back("bestvideo+bestaudio/best");
        }

        /* Use mp4 as the universal container for merged output.
         * Don't use the selected format's extension here because formats
         * like "mhtml" (or other non-standard containers) are not valid
         * merge output targets for yt-dlp. */
        args.push_back("--merge-output-format");
        args.push_back("mp4");
    }

    /* Subtitle options */
    auto subtitles = task.selectedSubtitles();
    if (!subtitles.empty())
    {
        args.push_back("--write-subs");
        args.push_back("--sub-langs");
        owners.emplace_back();
        for (size_t i = 0; i < subtitles.size(); ++i)
        {
            if (i > 0) owners.back() += ",";
            owners.back() += subtitles[i]->language;
        }
        args.push_back(owners.back().c_str());

        if (task.embedSubtitles())
            args.push_back("--embed-subs");
    }

    /* Embed metadata (thumbnail, chapters) */
    if (task.embedMetadata())
    {
        args.push_back("--embed-thumbnail");
        args.push_back("--add-metadata");
    }

    if (task.embedChapters())
        args.push_back("--embed-chapters");

    /* Output template */
    args.push_back("-o");
    args.push_back(outputPath.c_str());

    /* Progress reporting (newline for line-by-line parsing) */
    args.push_back("--newline");
    args.push_back("--progress");
    args.push_back("--console-title");
    args.push_back("--no-quiet");

    /* The URL */
    args.push_back(task.url().c_str());
    args.push_back(nullptr);

    return result;
}

std::string YtdlpStrategy::extractError(const std::string& stderrOutput)
{
    if (stderrOutput.empty())
        return "yt-dlp failed with no error output";

    std::istringstream stream(stderrOutput);
    std::string line;
    while (std::getline(stream, line))
    {
        /* yt-dlp error lines contain "ERROR:" */
        auto pos = line.find("ERROR:");
        if (pos != std::string::npos)
        {
            /* Return everything after "ERROR: " */
            pos += 6; /* skip "ERROR:" */
            while (pos < line.size() && line[pos] == ' ')
                ++pos;
            return line.substr(pos);
        }
    }

    /* Fallback: return the last non-empty line */
    std::string lastLine;
    stream.clear();
    stream.seekg(0);
    while (std::getline(stream, line))
    {
        if (!line.empty())
            lastLine = line;
    }

    return lastLine.empty() ? "Unknown yt-dlp error" : lastLine;
}

IDownloadStrategy::Result YtdlpStrategy::execute(
    const DownloadTask& task,
    const std::string& outputPath,
    ProgressCallback onProgress,
    CancellationToken* token)
{
    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "YtdlpStrategy: starting download for %s -> %s",
                task.url().c_str(), outputPath.c_str());

    /* Build args — the ArgList struct keeps all backing strings alive */
    auto argList = buildArgs(task, outputPath);
    argList.args.back() = nullptr; /* ensure null-terminated */

    /* Populate opts from argList — pointers remain valid as long as argList lives */
    ProcessRunner::Options opts;
    opts.executablePath = m_ytdlpPath.c_str();
    opts.args = argList.args;
    opts.captureStdout = false;
    opts.captureStderr = true;
    opts.logger = m_vlcObj;

    fprintf(stderr, "[YtdlpStrategy] args:");
    fprintf(stderr, " %s", opts.executablePath);
    for (auto a : opts.args)
        if (a) fprintf(stderr, " '%s'", a);
    fprintf(stderr, "\n");

    ProcessRunner runner;

    if (!runner.start(opts))
    {
        std::string error = "Failed to start yt-dlp process";
        if (m_vlcObj)
            msg_Err(m_vlcObj, "YtdlpStrategy: %s", error.c_str());
        return Result{false, "", error};
    }

    /* Monitor progress via stderr line-by-line */
    std::string accumulatedStderr;

    while (runner.isRunning())
    {
        if (token && token->isCancelled())
        {
            runner.cancel();
            return Result{false, "", "Download cancelled"};
        }

        std::string line = runner.readStderrLine(token);
        if (line.empty())
        {
            /* readStderrLine returned empty either because:
             *   1. The pipe reached EOF (process exited normally)
             *   2. The cancellation token was set during an EAGAIN
             *      (no data available, but token cancelled)
             * In case 2, we need to cancel the runner and return.
             * In case 1, we fall through to wait(). */
            if (token && token->isCancelled())
            {
                runner.cancel();
                return Result{false, "", "Download cancelled"};
            }
            break;
        }

        accumulatedStderr += line + "\n";

        /* Parse progress from this line */
        auto progressData = YtdlpProgressParser::parseLine(line);
        if (progressData && onProgress)
        {
            onProgress(progressData->percent,
                       progressData->speed,
                       progressData->eta,
                       progressData->downloadedBytes,
                       progressData->totalBytes);
        }
    }

    /* Wait for process to finish and collect exit status */
    int exitStatus = runner.wait();

    /* Drain any remaining stderr */
    std::string remaining;
    while (!(remaining = runner.readStderrLine()).empty())
        accumulatedStderr += remaining + "\n";

    if (exitStatus == 0)
    {
        if (m_vlcObj)
            msg_Dbg(m_vlcObj, "YtdlpStrategy: download completed successfully");

        return Result{true, outputPath, ""};
    }

    std::string error = extractError(accumulatedStderr);

    if (m_vlcObj)
        msg_Err(m_vlcObj, "YtdlpStrategy: download failed (exit %d): %s",
                exitStatus, error.c_str());

    return Result{false, "", error};
}

} // namespace downloader
} // namespace vlc
