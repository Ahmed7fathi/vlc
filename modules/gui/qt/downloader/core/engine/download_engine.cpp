/*****************************************************************************
 * download_engine.cpp
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

#include "download_engine.hpp"
#include "../utils/file_manager.hpp"

#include <vlc_common.h>
#include <vlc_fs.h>

#include <cstdio> /* rename */

namespace vlc {
namespace downloader {

DownloadEngine::DownloadEngine(vlc_object_t* vlcObj, EventBus& eventBus)
    : m_vlcObj(vlcObj)
    , m_eventBus(eventBus)
{
}

DownloadEngine::~DownloadEngine()
{
    cancel();
    join();
}

void DownloadEngine::start(
    std::shared_ptr<DownloadTask> task,
    std::unique_ptr<IDownloadStrategy> strategy,
    ProcessingPipeline pipeline,
    const std::string& tempOutput,
    std::unique_ptr<TempFileManager> tempManager,
    const std::string& finalOutput,
    std::shared_ptr<CancellationToken> token)
{
    if (m_running.exchange(true))
    {
        if (m_vlcObj)
            msg_Warn(m_vlcObj, "DownloadEngine: already running, ignoring start()");
        return;
    }

    m_task = std::move(task);
    m_strategy = std::move(strategy);
    m_pipeline = std::move(pipeline);
    m_tempOutput = tempOutput;
    m_tempManager = std::move(tempManager);
    m_finalOutput = finalOutput;
    m_token = token ? std::move(token) : std::make_shared<CancellationToken>();

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadEngine: starting download thread for %s -> %s",
                m_task->url().c_str(), m_finalOutput.c_str());

    m_thread = std::thread(&DownloadEngine::runThread, this);
}

void DownloadEngine::cancel()
{
    if (m_token)
        m_token->cancel();
}

void DownloadEngine::join()
{
    if (m_thread.joinable())
        m_thread.join();
}

void DownloadEngine::runThread()
{
    if (!m_task || !m_strategy)
    {
        if (m_vlcObj)
            msg_Err(m_vlcObj, "DownloadEngine: no task or strategy configured");
        m_running = false;
        return;
    }

    /* ── Phase 1: Download ─────────────────────────────────────────────── */
    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadEngine: phase 1 - downloading via %s",
                m_strategy->name().c_str());

    if (m_token->isCancelled())
    {
        if (m_vlcObj)
            msg_Dbg(m_vlcObj, "DownloadEngine: cancelled before download started");
        m_task->setError("Download cancelled");
        m_task->transitionTo(DownloadTask::State::Failed);
        m_eventBus.publish(DownloadFailed{m_task, "Download cancelled"});
        m_running = false;
        return;
    }

    /* Create parent directory for output */
    if (!m_finalOutput.empty())
    {
        FileManager::ensureParentDirectories(m_finalOutput);
    }

    /* Run the download strategy with progress callback */
    auto downloadResult = m_strategy->execute(
        *m_task,
        m_tempOutput,
        /* Progress callback */
        [this](int percent, double speed, int64_t eta,
               int64_t downloadedBytes, int64_t totalBytes) {
            m_task->updateProgress(percent, speed, eta,
                                   downloadedBytes, totalBytes);

            m_eventBus.publish(DownloadProgressEvent{
                m_task, percent, speed, eta,
                downloadedBytes, totalBytes
            });
        },
        m_token.get());

    if (!downloadResult.succeeded)
    {
        std::string error = downloadResult.errorMessage;
        m_task->setError(error);
        m_task->transitionTo(DownloadTask::State::Failed);
        m_eventBus.publish(DownloadFailed{m_task, error});

        if (m_vlcObj)
            msg_Err(m_vlcObj, "DownloadEngine: download failed: %s", error.c_str());

        m_running = false;
        return;
    }

    if (m_token->isCancelled())
    {
        m_task->setError("Download cancelled after completion");
        m_task->transitionTo(DownloadTask::State::Cancelled);
        m_running = false;
        return;
    }

    /* ── Phase 2: Post-processing ──────────────────────────────────────── */
    if (!m_pipeline.empty())
    {
        if (m_vlcObj)
            msg_Dbg(m_vlcObj, "DownloadEngine: phase 2 - post-processing (%zu steps)",
                    m_pipeline.stepCount());

        /* Use the temp output from the download, process to a temporary intermediate */
        auto processingResult = m_pipeline.process(
            m_tempOutput, m_finalOutput, *m_task, m_token.get());

        if (!processingResult.succeeded)
        {
            std::string error = "Post-processing failed: " + processingResult.errorMessage;
            m_task->setError(error);
            m_task->transitionTo(DownloadTask::State::Failed);
            m_eventBus.publish(DownloadFailed{m_task, error});

            if (m_vlcObj)
                msg_Err(m_vlcObj, "DownloadEngine: %s", error.c_str());

            m_running = false;
            return;
        }

        if (m_token->isCancelled())
        {
            m_task->setError("Download cancelled during post-processing");
            m_task->transitionTo(DownloadTask::State::Cancelled);
            m_running = false;
            return;
        }

        /* Remove the temporary download file since processing created the final output */
        vlc_unlink(m_tempOutput.c_str());
    }
    else
    {
        /* No post-processing — move temp file to final destination */
        if (!m_tempOutput.empty() && !m_finalOutput.empty() &&
            m_tempOutput != m_finalOutput)
        {
            if (vlc_rename(m_tempOutput.c_str(), m_finalOutput.c_str()) != 0)
            {
                /* Rename failed — try copy + unlink */
                if (m_vlcObj)
                    msg_Warn(m_vlcObj, "DownloadEngine: rename failed, copying instead");

                /* Fallback: just use the temp path as the final output */
                /* The TempFileManager in the orchestrator will handle cleanup */
                if (m_vlcObj)
                    msg_Warn(m_vlcObj, "DownloadEngine: using temp path as output: %s",
                            m_tempOutput.c_str());
            }
        }
    }

    /* ── Phase 3: Complete ─────────────────────────────────────────────── */
    m_task->setCompletedAt(std::chrono::system_clock::now());
    m_task->setCurrentFile(m_finalOutput);
    m_task->updateProgress(100, 0.0, 0,
                           m_task->totalBytes(), m_task->totalBytes());
    m_task->transitionTo(DownloadTask::State::Completed);

    m_eventBus.publish(DownloadCompleted{m_task});

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadEngine: download completed: %s",
                m_finalOutput.c_str());

    m_running = false;

    fprintf(stderr, "[DownloadEngine] download completed: %s\n",
            m_finalOutput.c_str());

    /* Detach the thread before notifying the completion callback.
     * The callback may trigger destruction of this DownloadEngine
     * (e.g., by erasing it from the active engines map), which would
     * call ~DownloadEngine() → join(). Joining our own thread causes
     * EDEADLK ("Resource deadlock avoided"). By detaching first,
     * join() becomes a safe no-op since the thread is no longer joinable. */
    if (m_thread.joinable())
        m_thread.detach();

    /* Notify completion callback */
    if (m_onComplete)
        m_onComplete(true, m_finalOutput);
}

} // namespace downloader
} // namespace vlc
