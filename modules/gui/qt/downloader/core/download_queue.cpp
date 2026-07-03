/*****************************************************************************
 * download_queue.cpp
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

#include "download_queue.hpp"
#include "../models/download_state_machine.hpp"

#include <algorithm>

namespace vlc {
namespace downloader {

using State = DownloadStateMachine::State;

DownloadQueue::DownloadQueue(EventBus& eventBus, unsigned int maxConcurrent)
    : m_eventBus(eventBus)
    , m_maxConcurrent(maxConcurrent)
{
}

void DownloadQueue::addTask(std::shared_ptr<DownloadTask> task)
{
    if (!task)
        return;

    /* Keep a reference for the callback registration before the move */
    auto taskRef = task;

    /* Register callback for state changes so the EventBus gets notified
     * even before enqueue() is called (e.g., during analysis). */
    taskRef->onStateChanged([this, weakTask = std::weak_ptr<DownloadTask>(taskRef)]
                         (State oldState, State newState)
    {
        auto strongTask = weakTask.lock();
        if (!strongTask)
            return;

        m_eventBus.publish(TaskStateChanged{strongTask, oldState, newState});

        /* Terminal states: clean up and process next */
        if (newState == State::Completed || newState == State::Failed ||
            newState == State::Cancelled)
        {
            if (!strongTask->completedAt().time_since_epoch().count())
                strongTask->setCompletedAt(std::chrono::system_clock::now());
            processNext();
        }
    });

    m_tasks.push_back(std::move(task));
    m_eventBus.publish(TaskCreated{std::move(taskRef)});
}

bool DownloadQueue::enqueue(std::shared_ptr<DownloadTask> task)
{
    if (!task)
        return false;

    if (!task->transitionTo(State::Queued))
        return false;

    /* The onStateChanged callback is already registered by addTask() — it handles
     * publishing TaskStateChanged events, setting completedAt on terminal states,
     * and calling processNext(). We must NOT register a second callback here.
     *
     * The task was also already added to m_tasks and TaskCreated was already
     * published by addTask(). Doing either again would cause the task to be
     * double-counted in activeCount(). */

    /* Try to start processing immediately */
    processNext();
    return true;
}

bool DownloadQueue::pause(const std::string& taskId)
{
    auto task = findTask(taskId);
    if (!task)
        return false;

    return task->transitionTo(State::Paused);
}

bool DownloadQueue::resume(const std::string& taskId)
{
    auto task = findTask(taskId);
    if (!task)
        return false;

    /* Re-queue the paused task */
    if (!task->transitionTo(State::Queued))
        return false;

    processNext();
    return true;
}

bool DownloadQueue::cancel(const std::string& taskId)
{
    auto task = findTask(taskId);
    if (!task || task->isTerminal())
        return false;

    /* Try to transition to Cancelled. If the current state doesn't
     * allow it directly (e.g., Downloading), the caller may need to
     * cancel the underlying process first. We still mark it cancelled
     * so the engine can check isTerminal(). */
    bool ok = task->transitionTo(State::Cancelled);
    if (ok)
    {
        task->setCompletedAt(std::chrono::system_clock::now());
        processNext();
    }
    return ok;
}

bool DownloadQueue::retry(const std::string& taskId)
{
    auto task = findTask(taskId);
    if (!task)
        return false;

    /* Clear the error and re-queue */
    task->setError({});

    if (!task->transitionTo(State::Queued))
        return false;

    processNext();
    return true;
}

void DownloadQueue::clearCompleted()
{
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
                       [](const auto& task) { return task->isTerminal(); }),
        m_tasks.end());
}

size_t DownloadQueue::activeCount() const
{
    return std::count_if(m_tasks.begin(), m_tasks.end(),
                         [](const auto& task) {
                             return task->state() == State::Downloading
                                 || task->state() == State::PostProcessing;
                         });
}

size_t DownloadQueue::queuedCount() const
{
    return std::count_if(m_tasks.begin(), m_tasks.end(),
                         [](const auto& task) {
                             return task->state() == State::Queued;
                         });
}

std::shared_ptr<DownloadTask> DownloadQueue::findTask(const std::string& taskId) const
{
    for (const auto& task : m_tasks)
    {
        if (task->id() == taskId)
            return task;
    }
    return nullptr;
}

void DownloadQueue::processNext()
{
    /* Start as many queued tasks as concurrent slots allow */
    for (auto& task : m_tasks)
    {
        if (activeCount() >= m_maxConcurrent)
            break;

        if (task->state() == State::Queued)
        {
            if (task->transitionTo(State::Downloading) && m_processNextCb)
                m_processNextCb(task);
            /* Continue looping to fill remaining concurrent slots */
        }
    }
}

} // namespace downloader
} // namespace vlc
