/*****************************************************************************
 * download_task.cpp
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

#include "download_task.hpp"

#include <atomic>
#include <chrono>
#include <string>

namespace vlc {
namespace downloader {

namespace {

/** Monotonically increasing counter for unique task IDs */
std::atomic<uint64_t> s_nextId{0};

} // anonymous namespace

std::shared_ptr<DownloadTask> DownloadTask::create(const std::string& url)
{
    /* Can't use std::make_shared because the constructor is private.
     * The shared_ptr is required for std::enable_shared_from_this. */
    return std::shared_ptr<DownloadTask>(new DownloadTask(url));
}

DownloadTask::DownloadTask(const std::string& url)
    : m_id(generateId())
    , m_url(url)
    , m_createdAt(std::chrono::system_clock::now())
{
}

DownloadTask::Id DownloadTask::generateId()
{
    /* Unique across the application lifetime:
     *   <timestamp_ms>-<counter> */
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return std::to_string(now) + "-" + std::to_string(s_nextId++);
}

void DownloadTask::updateProgress(int percent, double speedBytes, int64_t etaSecs,
                                  int64_t downloaded, int64_t total)
{
    m_progress.store(percent);
    m_speed.store(speedBytes);
    m_eta.store(etaSecs);
    m_downloadedBytes.store(downloaded);
    m_totalBytes.store(total);
}

} // namespace downloader
} // namespace vlc
