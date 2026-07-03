/*****************************************************************************
 * download_engine.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_ENGINE_HPP
#define VLC_DOWNLOADER_DOWNLOAD_ENGINE_HPP

#include "../../models/download_task.hpp"
#include "../../core/interfaces/i_download_strategy.hpp"
#include "../../core/processors/processing_pipeline.hpp"
#include "../../core/events/event_bus.hpp"
#include "../../core/events/download_events.hpp"
#include "../../core/utils/cancellation_token.hpp"
#include "../../core/utils/temp_file_manager.hpp"

#include <memory>
#include <string>
#include <atomic>
#include <thread>

struct vlc_object_t;

namespace vlc {
namespace downloader {

/**
 * @brief Coordinates the full download lifecycle for a single task.
 *
 * The DownloadEngine manages:
 *   1. Download execution via IDownloadStrategy (e.g., yt-dlp)
 *   2. Progress monitoring and event publishing
 *   3. Post-processing via ProcessingPipeline
 *   4. Cancellation support
 *   5. Output file management (temp file → final destination)
 *
 * Each DownloadEngine instance handles one download task at a time.
 * Multiple engines can run concurrently for parallel downloads.
 *
 * Usage (typically via DownloadOrchestrator):
 * @code
 *   auto engine = std::make_unique<DownloadEngine>(vlcObj, eventBus);
 *   engine->start(task, strategy, pipeline, outputPath, token);
 * @endcode
 */
class DownloadEngine
{
public:
    /**
     * @brief Create a DownloadEngine.
     *
     * @param vlcObj    VLC object for logging.
     * @param eventBus  Event bus for publishing progress/completion events.
     */
    explicit DownloadEngine(vlc_object_t* vlcObj, EventBus& eventBus);
    ~DownloadEngine();

    /** Non-copyable, non-movable */
    DownloadEngine(const DownloadEngine&) = delete;
    DownloadEngine& operator=(const DownloadEngine&) = delete;
    DownloadEngine(DownloadEngine&&) = delete;
    DownloadEngine& operator=(DownloadEngine&&) = delete;

    /**
     * @brief Start a download asynchronously on a background thread.
     *
     * @param task          The download task to execute.
     * @param strategy      Download strategy (e.g., YtdlpStrategy).
     * @param pipeline      Post-processing pipeline (empty = no post-processing).
     * @param tempOutput    Temp path for initial download output.
     * @param tempManager   TempFileManager owning the temp file (kept alive during download).
     * @param finalOutput   Final output path (after processing/move).
     * @param token         Cancellation token (shared with UI for cancel).
     */
    void start(std::shared_ptr<DownloadTask> task,
               std::unique_ptr<IDownloadStrategy> strategy,
               ProcessingPipeline pipeline,
               const std::string& tempOutput,
               std::unique_ptr<TempFileManager> tempManager,
               const std::string& finalOutput,
               std::shared_ptr<CancellationToken> token);

    /** @brief Whether the engine is currently running a download. */
    bool isRunning() const { return m_running.load(); }

    /**
     * @brief Cancel the current download.
     * Cancels the running process via the cancellation token.
     * Non-blocking; safe to call from any thread.
     */
    void cancel();

    /**
     * @brief Wait for the download thread to finish.
     * Returns immediately if no download is running.
     */
    void join();

    /** @brief Get the task being processed. */
    std::shared_ptr<DownloadTask> task() const { return m_task; }

    /**
     * @brief Callback for when the engine completes a download operation.
     * Called after the download thread finishes (success or failure).
     */
    using CompletionCallback = std::function<void(bool succeeded, const std::string& outputPath)>;
    void onComplete(CompletionCallback cb) { m_onComplete = std::move(cb); }

private:
    /** The main download thread function. */
    void runThread();

    vlc_object_t* m_vlcObj;
    EventBus& m_eventBus;

    std::shared_ptr<DownloadTask> m_task;
    std::unique_ptr<IDownloadStrategy> m_strategy;
    ProcessingPipeline m_pipeline;
    std::string m_tempOutput;
    std::string m_finalOutput;
    std::shared_ptr<CancellationToken> m_token;
    std::unique_ptr<TempFileManager> m_tempManager;

    std::atomic<bool> m_running{false};
    std::thread m_thread;
    CompletionCallback m_onComplete;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_ENGINE_HPP
