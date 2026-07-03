/*****************************************************************************
 * download_queue.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_QUEUE_HPP
#define VLC_DOWNLOADER_DOWNLOAD_QUEUE_HPP

#include "../models/download_task.hpp"
#include "events/event_bus.hpp"
#include "events/download_events.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace vlc {
namespace downloader {

/**
 * @brief Manages the lifecycle of download tasks in a queue.
 *
 * The queue enforces state machine transitions and limits concurrent downloads.
 * Tasks move through states: Created → Analyzing → Ready → Queued → Downloading
 * → PostProcessing → Completed (or Failed/Cancelled at any point).
 *
 * Supports pause, resume, cancel, and retry operations.
 * Designed for single concurrent download initially, but supports
 * maxConcurrent > 1 for future playlist/batch downloads.
 */
class DownloadQueue
{
public:
    /**
     * @brief Create a DownloadQueue.
     *
     * @param eventBus         Event bus for publishing lifecycle events.
     * @param maxConcurrent    Maximum number of simultaneous downloads.
     */
    explicit DownloadQueue(EventBus& eventBus, unsigned int maxConcurrent = 1);
    ~DownloadQueue() = default;

    /** Non-copyable, movable */
    DownloadQueue(const DownloadQueue&) = delete;
    DownloadQueue& operator=(const DownloadQueue&) = delete;
    DownloadQueue(DownloadQueue&&) = default;
    DownloadQueue& operator=(DownloadQueue&&) = default;

    // ── Queue operations ─────────────────────────────────────────────────

    /**
     * @brief Add a pre-analysis task to the queue for tracking.
     *
     * Unlike enqueue(), this does NOT require the task to be in Ready state.
     * It simply registers the task in the queue so it can be found via
     * findTask() and tracked during analysis. The task stays in its
     * current state (Created/Analyzing).
     */
    void addTask(std::shared_ptr<DownloadTask> task);

    /**
     * @brief Add a task to the queue.
     *
     * The task must be in Ready state. Transitions to Queued.
     * If a download slot is available, starts immediately.
     */
    bool enqueue(std::shared_ptr<DownloadTask> task);

    /**
     * @brief Pause a running download.
     * @return true if the task was in Downloading state and is now Paused.
     */
    bool pause(const std::string& taskId);

    /**
     * @brief Resume a paused download (re-queues it).
     * @return true if the task was in Paused state and is now Queued.
     */
    bool resume(const std::string& taskId);

    /**
     * @brief Cancel a task regardless of its state.
     * Transitions to Cancelled if not in a terminal state.
     */
    bool cancel(const std::string& taskId);

    /**
     * @brief Retry a failed task (re-queues it).
     * @return true if the task was in Failed state and is now Queued.
     */
    bool retry(const std::string& taskId);

    /** @brief Remove all completed/cancelled/failed tasks from the queue. */
    void clearCompleted();

    // ── Callbacks ────────────────────────────────────────────────────────

    /**
     * @brief Callback when a task is ready to start downloading.
     *
     * The queue calls this when a queued task reaches the front and
     * a download slot is available. The client (DownloadOrchestrator)
     * should begin the actual download.
     */
    using ProcessNextCallback = std::function<void(std::shared_ptr<DownloadTask>)>;
    void onProcessNext(ProcessNextCallback cb) { m_processNextCb = std::move(cb); }

    // ── State queries ────────────────────────────────────────────────────

    /** @brief Number of tasks currently being downloaded. */
    size_t activeCount() const;

    /** @brief Number of tasks waiting in the queue. */
    size_t queuedCount() const;

    /** @brief Total number of tasks in the queue (all states). */
    size_t totalCount() const { return m_tasks.size(); }

    /** @brief Get all tasks. */
    const std::vector<std::shared_ptr<DownloadTask>>& tasks() const { return m_tasks; }

    /** @brief Find a task by ID. */
    std::shared_ptr<DownloadTask> findTask(const std::string& taskId) const;

    /** @brief Maximum concurrent downloads. */
    unsigned int maxConcurrent() const { return m_maxConcurrent; }
    void setMaxConcurrent(unsigned int max) { m_maxConcurrent = max; }

private:
    /** Process the next task in the queue if a slot is available. */
    void processNext();

    EventBus& m_eventBus;
    std::vector<std::shared_ptr<DownloadTask>> m_tasks;
    unsigned int m_maxConcurrent;
    ProcessNextCallback m_processNextCb;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_QUEUE_HPP
