/*****************************************************************************
 * download_task.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_TASK_HPP
#define VLC_DOWNLOADER_DOWNLOAD_TASK_HPP

#include "download_state_machine.hpp"
#include "media_info.hpp"

#include <memory>
#include <string>
#include <chrono>
#include <atomic>

namespace vlc {
namespace downloader {

/**
 * @brief Represents a single download job from creation to completion.
 *
 * DownloadTask is the central unit of work in the downloader framework.
 * It is managed through shared_ptr because it is owned by multiple components
 * simultaneously (the queue while active, event callbacks, the UI model).
 *
 * The state machine embedded within enforces a valid lifecycle.
 * All state queries delegate to DownloadStateMachine; direct enum mutation
 * is impossible from outside the class.
 */
class DownloadTask : public std::enable_shared_from_this<DownloadTask>
{
public:
    /** Shorthand for the state enum */
    using State = DownloadStateMachine::State;

    /** Unique identifier type */
    using Id = std::string;

    /**
     * @brief Factory method. Creates a task in Created state.
     * @param url  The source URL to download.
     * @return A shared_ptr to the new task.
     */
    static std::shared_ptr<DownloadTask> create(const std::string& url);

    /** @brief Destructor */
    ~DownloadTask() = default;

    /** Non-copyable, non-movable — tasks are identity-bound */
    DownloadTask(const DownloadTask&) = delete;
    DownloadTask& operator=(const DownloadTask&) = delete;
    DownloadTask(DownloadTask&&) = delete;
    DownloadTask& operator=(DownloadTask&&) = delete;

    // ── Identity ─────────────────────────────────────────────────────────

    /** @brief Unique identifier for this task (generated at creation). */
    const Id& id() const { return m_id; }

    /** @brief The source URL. */
    const std::string& url() const { return m_url; }

    // ── State machine ────────────────────────────────────────────────────

    /** @brief Current state. */
    State state() const { return m_stateMachine.currentState(); }

    /**
     * @brief Attempt a state transition.
     * @return true if the transition was valid and performed.
     * @note This is the ONLY way to change the task state. */
    bool transitionTo(State newState) { return m_stateMachine.transitionTo(newState); }

    /** @brief True if the task has reached a terminal state. */
    bool isTerminal() const { return m_stateMachine.isTerminal(); }

    /** @brief True if the task is in a non-terminal, actionable state. */
    bool isActive() const { return m_stateMachine.isActive(); }

    /** @brief Human-readable current state name. */
    const char* stateName() const { return m_stateMachine.stateName(); }

    /**
     * @brief Register a callback for state changes.
     * @note Callbacks are invoked synchronously during transitionTo(). */
    void onStateChanged(DownloadStateMachine::StateChangeCallback cb)
    {
        m_stateMachine.onStateChanged(std::move(cb));
    }

    // ── Media info (set after analysis) ──────────────────────────────────

    /** @brief Access the analysis result. May be null before analysis completes. */
    const MediaInfo* mediaInfo() const { return m_mediaInfo.get(); }

    /** @brief Store the analysis result. Transfers ownership. */
    void setMediaInfo(std::unique_ptr<MediaInfo> info) { m_mediaInfo = std::move(info); }

    // ── User selections (set before downloading) ─────────────────────────

    /** Sentinel value meaning "no format selected" or "not applicable". */
    static constexpr size_t NO_SELECTION = ~size_t(0);

    /**
     * @brief The selected video format index into mediaInfo().videoFormats.
     * @return A pointer to the format, or nullptr if not selected or mediaInfo is null.
     */
    const VideoFormat* selectedVideoFormat() const
    {
        if (!m_mediaInfo || m_selectedVideoFormatIdx >= m_mediaInfo->videoFormats.size())
            return nullptr;
        return &m_mediaInfo->videoFormats[m_selectedVideoFormatIdx];
    }

    /** @brief Select a video format by index into mediaInfo().videoFormats. */
    void selectVideoFormat(size_t index) { m_selectedVideoFormatIdx = index; }

    /** @brief The selected audio format index into mediaInfo().audioFormats. */
    const AudioFormat* selectedAudioFormat() const
    {
        if (!m_mediaInfo || m_selectedAudioFormatIdx >= m_mediaInfo->audioFormats.size())
            return nullptr;
        return &m_mediaInfo->audioFormats[m_selectedAudioFormatIdx];
    }

    /** @brief Select an audio format by index into mediaInfo().audioFormats. */
    void selectAudioFormat(size_t index) { m_selectedAudioFormatIdx = index; }

    /**
     * @brief Indices of selected subtitle tracks into mediaInfo().subtitles.
     * @return A vector of SubtitleTrack pointers, or empty if none selected.
     */
    std::vector<const SubtitleTrack*> selectedSubtitles() const
    {
        std::vector<const SubtitleTrack*> result;
        if (!m_mediaInfo)
            return result;
        for (auto idx : m_selectedSubtitleIndices)
        {
            if (idx < m_mediaInfo->subtitles.size())
                result.push_back(&m_mediaInfo->subtitles[idx]);
        }
        return result;
    }

    /** @brief Add a subtitle track to download by index into mediaInfo().subtitles. */
    void addSubtitle(size_t index) { m_selectedSubtitleIndices.push_back(index); }

    /** @brief Clear subtitle selection. */
    void clearSubtitles() { m_selectedSubtitleIndices.clear(); }

    // ── Options ──────────────────────────────────────────────────────────

    /** @brief Whether to embed subtitles into the output file. */
    bool embedSubtitles() const { return m_embedSubtitles; }
    void setEmbedSubtitles(bool val) { m_embedSubtitles = val; }

    /** @brief Whether to embed chapter markers into the output file. */
    bool embedChapters() const { return m_embedChapters; }
    void setEmbedChapters(bool val) { m_embedChapters = val; }

    /** @brief Whether to embed metadata (title, uploader, description, thumbnail). */
    bool embedMetadata() const { return m_embedMetadata; }
    void setEmbedMetadata(bool val) { m_embedMetadata = val; }

    /** @brief Whether to download only audio (no video stream). */
    bool audioOnly() const { return m_audioOnly; }
    void setAudioOnly(bool val) { m_audioOnly = val; }

    // ── Output ───────────────────────────────────────────────────────────

    /** @brief Output file path (set before download starts). */
    const std::string& outputPath() const { return m_outputPath; }
    void setOutputPath(const std::string& path) { m_outputPath = path; }

    /** @brief Filename template (e.g., "%(title)s.%(ext)s"). */
    const std::string& filenameTemplate() const { return m_filenameTemplate; }
    void setFilenameTemplate(const std::string& tpl) { m_filenameTemplate = tpl; }

    // ── Progress ─────────────────────────────────────────────────────────

    /** @brief Download progress percentage (0–100). */
    int progress() const { return m_progress.load(); }

    /** @brief Current download speed in bytes/sec. */
    double speed() const { return m_speed.load(); }

    /** @brief Estimated time remaining in seconds. */
    int64_t eta() const { return m_eta.load(); }

    /** @brief Bytes downloaded so far. */
    int64_t downloadedBytes() const { return m_downloadedBytes.load(); }

    /** @brief Total bytes to download (0 if unknown). */
    int64_t totalBytes() const { return m_totalBytes.load(); }

    /** @brief Name of the file currently being processed. */
    const std::string& currentFile() const { return m_currentFile; }
    void setCurrentFile(const std::string& file) { m_currentFile = file; }

    /**
     * @brief Update progress atomically.
     * @note Thread-safe — intended for calling from the download worker thread.
     */
    void updateProgress(int percent, double speedBytes, int64_t etaSecs,
                        int64_t downloaded, int64_t total);

    // ── Error ────────────────────────────────────────────────────────────

    /** @brief Error message if the task has failed. Empty otherwise. */
    const std::string& errorMessage() const { return m_errorMessage; }
    void setError(const std::string& message) { m_errorMessage = message; }

    // ── Timestamps ───────────────────────────────────────────────────────

    /** @brief When the task was created. */
    std::chrono::system_clock::time_point createdAt() const { return m_createdAt; }

    /** @brief When the task reached a terminal state. Zero-initialized if not yet terminal. */
    std::chrono::system_clock::time_point completedAt() const { return m_completedAt; }
    void setCompletedAt(std::chrono::system_clock::time_point tp) { m_completedAt = tp; }

private:
    explicit DownloadTask(const std::string& url);

    static Id generateId();

    // Identity
    Id m_id;
    std::string m_url;

    // State machine
    DownloadStateMachine m_stateMachine;

    // Analysis result
    std::unique_ptr<MediaInfo> m_mediaInfo;

    // User selections (indices into m_mediaInfo's vectors; safer than raw pointers)
    size_t m_selectedVideoFormatIdx = NO_SELECTION;
    size_t m_selectedAudioFormatIdx = NO_SELECTION;
    std::vector<size_t> m_selectedSubtitleIndices;

    // Options
    bool m_embedSubtitles = false;
    bool m_embedChapters = false;
    bool m_embedMetadata = true;
    bool m_audioOnly = false;

    // Output
    std::string m_outputPath;
    std::string m_filenameTemplate;

    // Progress (atomic for thread-safe access from worker threads)
    std::atomic<int> m_progress{0};
    std::atomic<double> m_speed{0.0};
    std::atomic<int64_t> m_eta{0};
    std::atomic<int64_t> m_downloadedBytes{0};
    std::atomic<int64_t> m_totalBytes{0};
    std::string m_currentFile;

    // Error
    std::string m_errorMessage;

    // Timestamps
    std::chrono::system_clock::time_point m_createdAt;
    std::chrono::system_clock::time_point m_completedAt;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_TASK_HPP
