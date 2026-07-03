/*****************************************************************************
 * download_events.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_EVENTS_HPP
#define VLC_DOWNLOADER_DOWNLOAD_EVENTS_HPP

#include "../../models/download_task.hpp"
#include "../../models/media_info.hpp"

#include <memory>
#include <string>

namespace vlc {
namespace downloader {

// ── Event types ─────────────────────────────────────────────────────────

/** Published when a new task is created. */
struct TaskCreated
{
    std::shared_ptr<DownloadTask> task;
};

/** Published when a task's state changes. */
struct TaskStateChanged
{
    std::shared_ptr<DownloadTask> task;
    DownloadStateMachine::State oldState;
    DownloadStateMachine::State newState;
};

/** Published when URL analysis completes successfully. */
struct AnalysisCompleted
{
    std::shared_ptr<DownloadTask> task;
};

/** Published when URL analysis fails. */
struct AnalysisFailed
{
    std::shared_ptr<DownloadTask> task;
    std::string errorMessage;
};

/** Published periodically during download to report progress. */
struct DownloadProgressEvent
{
    std::shared_ptr<DownloadTask> task;
    int percent;
    double speed;
    int64_t eta;
    int64_t downloadedBytes;
    int64_t totalBytes;
};

/** Published when download completes successfully. */
struct DownloadCompleted
{
    std::shared_ptr<DownloadTask> task;
};

/** Published when download fails. */
struct DownloadFailed
{
    std::shared_ptr<DownloadTask> task;
    std::string errorMessage;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_EVENTS_HPP
