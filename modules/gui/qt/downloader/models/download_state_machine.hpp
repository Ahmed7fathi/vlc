/*****************************************************************************
 * download_state_machine.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_STATE_MACHINE_HPP
#define VLC_DOWNLOADER_DOWNLOAD_STATE_MACHINE_HPP

#include <vector>
#include <functional>
#include <cstdint>

namespace vlc {
namespace downloader {

/**
 * @brief Explicit state machine governing the lifecycle of a DownloadTask.
 *
 * Every DownloadTask embeds one of these to enforce valid state transitions.
 * Illegal transitions are rejected at runtime with a boolean return value.
 *
 * State diagram:
 *
 *   Created ──► Analyzing ──► Ready ──► Queued ──► Downloading
 *     │            │            │            │           │
 *     │            ├──► Failed  │            │           ├──► Paused
 *     │            │            │            │           │     │
 *     └──► Cancelled           │            │           │     └──► Queued
 *                               │            │           │
 *                               └──► Cancelled           ├──► PostProcessing
 *                                                         │     │
 *                                                         │     └──► Completed
 *                                                         │           │
 *                                                         ├──► Cancelled
 *                                                         └──► Failed
 *
 *   Terminal states: Completed, Cancelled
 *   All states:      Created, Analyzing, Ready, Queued, Downloading,
 *                    Paused, PostProcessing, Completed, Failed, Cancelled
 */
class DownloadStateMachine
{
public:
    /** Possible states for a download task */
    enum class State : uint8_t
    {
        Created,         /**< Task just created, not yet analyzed */
        Analyzing,       /**< Provider is extracting metadata */
        Ready,           /**< Analysis complete, awaiting user confirmation */
        Queued,          /**< User confirmed, waiting in queue */
        Downloading,     /**< Actively downloading media */
        Paused,          /**< Download paused by user */
        PostProcessing,  /**< Download complete, running post-processing (merge, convert, embed) */
        Completed,       /**< All steps finished successfully */
        Failed,          /**< An error occurred */
        Cancelled        /**< Cancelled by user */
    };

    /** Callback invoked when the state changes */
    using StateChangeCallback = std::function<void(State oldState, State newState)>;

    /** @brief Default constructor starts in Created state */
    DownloadStateMachine() = default;

    /** @brief Attempt a transition to @p newState.
     *  @return true if the transition was valid and performed, false otherwise.
     *  On success, all registered callbacks are invoked. */
    bool transitionTo(State newState);

    /** @brief Return the current state. */
    State currentState() const { return m_state; }

    /** @brief True if the task has reached a terminal state (Completed or Cancelled). */
    bool isTerminal() const;

    /** @brief True if the task is in a non-terminal, actionable state. */
    bool isActive() const;

    /** @brief Return a human-readable name for the current state. Never null. */
    const char* stateName() const;

    /** @brief Return a human-readable name for a given state. Never null. */
    static const char* stateName(State state);

    /** @brief Register a callback invoked on every successful state transition. */
    void onStateChanged(StateChangeCallback callback);

    /** @brief Remove all registered callbacks. */
    void clearCallbacks();

private:
    State m_state = State::Created;
    std::vector<StateChangeCallback> m_callbacks;

    /** @brief Internal transition table. Returns true if @p from -> @p to is valid. */
    static bool isValidTransition(State from, State to);
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_STATE_MACHINE_HPP
