/*****************************************************************************
 * download_orchestrator.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_ORCHESTRATOR_HPP
#define VLC_DOWNLOADER_DOWNLOAD_ORCHESTRATOR_HPP

#include "../models/download_task.hpp"
#include "../models/download_settings.hpp"
#include "provider_registry.hpp"
#include "download_queue.hpp"
#include "engine/download_engine.hpp"
#include "events/event_bus.hpp"
#include "events/download_events.hpp"

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>

struct vlc_object_t;

namespace vlc {
namespace downloader {

/**
 * @brief Central orchestrator for the download system.
 *
 * Coordinates the full flow:
 *   1. createTask(url) → DownloadTask in Created state
 *   2. analyzeTask(task) → finds provider, runs analysis → Ready or Failed
 *   3. confirmDownload(task) (user selects options) → Queued
 *   4. Queue processes automatically → Downloading → PostProcessing → Completed
 *
 * The DownloadOrchestrator owns the ProviderRegistry and DownloadQueue.
 * It bridges user actions (analyze, download, cancel) with the internal
 * provider and queue system.
 */
class DownloadOrchestrator
{
public:
    /**
     * @brief Create a DownloadOrchestrator.
     *
     * @param vlcObj       VLC object for config and logging.
     * @param eventBus     Event bus for publishing lifecycle events.
     * @param settings     User-configurable download settings.
     */
    explicit DownloadOrchestrator(vlc_object_t* vlcObj,
                                  EventBus& eventBus,
                                  const DownloadSettings& settings);
    ~DownloadOrchestrator() = default;

    /** Non-copyable, movable */
    DownloadOrchestrator(const DownloadOrchestrator&) = delete;
    DownloadOrchestrator& operator=(const DownloadOrchestrator&) = delete;
    DownloadOrchestrator(DownloadOrchestrator&&) = default;
    DownloadOrchestrator& operator=(DownloadOrchestrator&&) = default;

    // ── Provider registry access ─────────────────────────────────────────

    /** @brief Access the provider registry (for registration). */
    ProviderRegistry& providers() { return m_providers; }

    // ── Task management ──────────────────────────────────────────────────

    /**
     * @brief Create a new DownloadTask for a URL.
     * Task starts in Created state.
     */
    std::shared_ptr<DownloadTask> createTask(const std::string& url);

    /**
     * @brief Analyze a URL by finding the appropriate provider.
     *
     * Transitions: Created → Analyzing.
     * On success: Analyzing → Ready.
     * On failure: Analyzing → Failed.
     */
    void analyzeTask(std::shared_ptr<DownloadTask> task);

    /**
     * @brief Confirm a task for download (user has selected options).
     * Transitions: Ready → Queued.
     */
    bool confirmDownload(std::shared_ptr<DownloadTask> task);

    /**
     * @brief Cancel a task by ID.
     */
    bool cancelTask(const std::string& taskId);

    /**
     * @brief Pause a running download.
     */
    bool pauseTask(const std::string& taskId);

    /**
     * @brief Resume a paused download.
     */
    bool resumeTask(const std::string& taskId);

    /**
     * @brief Retry a failed download.
     */
    bool retryTask(const std::string& taskId);

    // ── Queue access ─────────────────────────────────────────────────────

    /** @brief Access the download queue. */
    DownloadQueue& queue() { return m_queue; }
    const DownloadQueue& queue() const { return m_queue; }

    /** @brief Access the settings. */
    const DownloadSettings& settings() const { return m_settings; }
    void setSettings(const DownloadSettings& s) { m_settings = s; }

private:
    /** Handle the next task that needs to be downloaded. */
    void onProcessNext(std::shared_ptr<DownloadTask> task);

    /** Handle engine completion: remove from active map, notify queue. */
    void onEngineComplete(const std::string& taskId, bool succeeded);

    /** Build a ProcessingPipeline based on task settings. */
    ProcessingPipeline createPipeline(const DownloadTask& task) const;

    vlc_object_t* m_vlcObj;
    EventBus& m_eventBus;
    ProviderRegistry m_providers;
    DownloadQueue m_queue;
    DownloadSettings m_settings;

    /** Map of taskId → active DownloadEngine for running downloads. */
    std::unordered_map<std::string, std::unique_ptr<DownloadEngine>> m_activeEngines;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_ORCHESTRATOR_HPP
