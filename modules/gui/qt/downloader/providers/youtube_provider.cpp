/*****************************************************************************
 * youtube_provider.cpp
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

#include "youtube_provider.hpp"
#include "../core/utils/process_runner.hpp"
#include "../core/parsers/ytdlp_json_parser.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>

#include <cstring>
#include <cstdio>
#include <string>

namespace vlc {
namespace downloader {

namespace {

/**
 * @brief Check if a URL belongs to YouTube.
 *
 * Matches the following patterns:
 *   - youtube.com/* (including www., m., music.)
 *   - youtu.be/*
 *   - youtube-nocookie.com/*
 */
static bool isYouTubeUrl(const std::string& url)
{
    const char* p = url.c_str();

    /* Skip protocol (http://, https://) */
    if (std::strncmp(p, "https://", 8) == 0)
        p += 8;
    else if (std::strncmp(p, "http://", 7) == 0)
        p += 7;

    /* Skip subdomain prefixes */
    if (std::strncmp(p, "www.", 4) == 0)
        p += 4;
    else if (std::strncmp(p, "m.", 2) == 0)
        p += 2;
    else if (std::strncmp(p, "music.", 6) == 0)
        p += 6;

    /* Check for known YouTube domains */
    if (std::strncmp(p, "youtube.com/", 12) == 0)
        return true;

    if (std::strncmp(p, "youtu.be/", 9) == 0)
        return true;

    /* youtube-nocookie.com (used for embeds) */
    if (std::strncmp(p, "youtube-nocookie.com/", 21) == 0)
        return true;

    return false;
}

} // anonymous namespace

YoutubeProvider::YoutubeProvider(vlc_object_t* vlcObj)
    : m_cancelToken(std::make_shared<CancellationToken>())
    , m_vlcObj(vlcObj)
{
}

YoutubeProvider::~YoutubeProvider()
{
    cancel();
}

bool YoutubeProvider::canHandle(const std::string& url) const
{
    return isYouTubeUrl(url);
}

void YoutubeProvider::analyze(std::shared_ptr<DownloadTask> task,
                              AnalyzeSuccessCallback onSuccess,
                              AnalyzeErrorCallback onError)
{
    if (!task || !onSuccess || !onError)
        return;

    std::string ytdlpPath = resolveYtdlpPath();
    if (ytdlpPath.empty())
    {
        onError(std::move(task), "yt-dlp not found. Check 'ytdl-path' configuration.");
        return;
    }

    /* Reset cancellation state for this analysis */
    m_cancelToken->reset();

    /* Build yt-dlp arguments for single JSON dump */
    ProcessRunner::Options opts;
    opts.executablePath = ytdlpPath.c_str();
    opts.args = {
        "--dump-single-json",
        "--no-download",
        "--no-warnings",
        "--skip-download",
        "--no-playlist",   /* Prevent yt-dlp from processing entire playlists */
        "--",
        task->url().c_str()
    };
    opts.captureStdout = true;
    opts.captureStderr = true;
    opts.logger = m_vlcObj;

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "YouTubeProvider: analyzing %s", task->url().c_str());

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "YouTubeProvider: spawning: %s", ytdlpPath.c_str());
    for (const auto& a : opts.args)
        if (m_vlcObj) msg_Dbg(m_vlcObj, "  arg: %s", a ? a : "(null)");

    fprintf(stderr, "[YouTubeProvider] starting ProcessRunner::run for %s\n", task->url().c_str());

    /* Run yt-dlp synchronously — this blocks the calling thread.
     * The caller (DownloadService) should invoke analyze() from a worker thread. */
    ProcessRunner::Result result = ProcessRunner::run(opts, m_cancelToken.get());

    fprintf(stderr, "[YouTubeProvider] ProcessRunner::run completed (exit=%d, succeeded=%d, stdout=%zu bytes, stderr=%zu bytes)\n",
            result.exitStatus, result.succeeded, result.stdOut.size(), result.stdErr.size());
    if (!result.stdErr.empty())
        fprintf(stderr, "[YouTubeProvider] stderr: %s\n", result.stdErr.c_str());

    /* Check if analysis was cancelled */
    if (m_cancelToken->isCancelled())
    {
        if (m_vlcObj)
            msg_Dbg(m_vlcObj, "YouTubeProvider: analysis cancelled for %s", task->url().c_str());
        onError(std::move(task), "Analysis cancelled");
        return;
    }

    if (!result.succeeded)
    {
        std::string errorMsg = "yt-dlp failed with exit code "
                             + std::to_string(result.exitStatus);
        if (!result.stdErr.empty())
        {
            /* Extract the first meaningful error line from stderr */
            size_t pos = result.stdErr.find("ERROR:");
            if (pos != std::string::npos)
            {
                size_t end = result.stdErr.find('\n', pos);
                if (end != std::string::npos)
                    errorMsg = result.stdErr.substr(pos + 6, end - pos - 6);
                else
                    errorMsg = result.stdErr.substr(pos + 6);
            }
            else if (!result.stdErr.empty())
            {
                errorMsg += ": " + result.stdErr;
            }
        }

        if (m_vlcObj)
            msg_Err(m_vlcObj, "YouTubeProvider: %s", errorMsg.c_str());
        onError(std::move(task), std::move(errorMsg));
        return;
    }

    /* Parse JSON output into MediaInfo */
    auto mediaInfo = YtdlpJsonParser::parse(result.stdOut);
    if (!mediaInfo)
    {
        std::string errorMsg = "Failed to parse yt-dlp output";
        if (m_vlcObj)
            msg_Err(m_vlcObj, "YouTubeProvider: %s", errorMsg.c_str());
        onError(std::move(task), std::move(errorMsg));
        return;
    }

    /* Ensure required fields are populated */
    if (mediaInfo->extractor.empty())
        mediaInfo->extractor = "youtube";
    if (mediaInfo->url.empty())
        mediaInfo->url = task->url();

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "YouTubeProvider: analysis complete for '%s' (%llds, %zu subtitles)",
                mediaInfo->title.c_str(),
                static_cast<long long>(mediaInfo->duration),
                mediaInfo->subtitles.size());

    onSuccess(std::move(task), std::move(mediaInfo));
}

void YoutubeProvider::cancel()
{
    m_cancelToken->cancel();
}

std::string YoutubeProvider::resolveYtdlpPath() const
{
    if (!m_vlcObj)
    {
        /* Fallback: check PATH */
        return "yt-dlp";
    }

    /* ytdl-path is defined in modules/demux/ytdl.c, not in core libvlc-module.c.
     * It may not exist at startup if the ytdl demux module hasn't been loaded yet.
     * Guard with config_FindConfig to avoid asserting in config_GetPsz. */
    if (!config_FindConfig("ytdl-path"))
        return "yt-dlp";

    char* path = var_InheritString(m_vlcObj, "ytdl-path");
    if (!path)
        return "yt-dlp";

    std::string result(path);
    free(path);
    return result;
}

} // namespace downloader
} // namespace vlc
