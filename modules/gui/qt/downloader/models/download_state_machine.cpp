/*****************************************************************************
 * download_state_machine.cpp
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

#include "download_state_machine.hpp"

#include <cassert>

namespace vlc {
namespace downloader {

bool DownloadStateMachine::transitionTo(State newState)
{
    if (!isValidTransition(m_state, newState))
        return false;

    State oldState = m_state;
    m_state = newState;

    for (auto& cb : m_callbacks)
    {
        if (cb)
            cb(oldState, newState);
    }

    return true;
}

bool DownloadStateMachine::isTerminal() const
{
    return m_state == State::Completed || m_state == State::Cancelled;
}

bool DownloadStateMachine::isActive() const
{
    return !isTerminal() && m_state != State::Created;
}

const char* DownloadStateMachine::stateName() const
{
    return stateName(m_state);
}

const char* DownloadStateMachine::stateName(State state)
{
    switch (state)
    {
        case State::Created:         return "Created";
        case State::Analyzing:       return "Analyzing";
        case State::Ready:           return "Ready";
        case State::Queued:          return "Queued";
        case State::Downloading:     return "Downloading";
        case State::Paused:          return "Paused";
        case State::PostProcessing:  return "PostProcessing";
        case State::Completed:       return "Completed";
        case State::Failed:          return "Failed";
        case State::Cancelled:       return "Cancelled";
    }
    return "Unknown";
}

void DownloadStateMachine::onStateChanged(StateChangeCallback callback)
{
    if (callback)
        m_callbacks.push_back(std::move(callback));
}

void DownloadStateMachine::clearCallbacks()
{
    m_callbacks.clear();
}

bool DownloadStateMachine::isValidTransition(State from, State to)
{
    using S = State;

    switch (from)
    {
        case S::Created:
            return to == S::Analyzing || to == S::Cancelled;

        case S::Analyzing:
            return to == S::Ready || to == S::Failed || to == S::Cancelled;

        case S::Ready:
            return to == S::Queued || to == S::Cancelled;

        case S::Queued:
            return to == S::Downloading || to == S::Cancelled;

        case S::Downloading:
            return to == S::Paused || to == S::PostProcessing
                || to == S::Completed
                || to == S::Failed || to == S::Cancelled;

        case S::Paused:
            return to == S::Queued || to == S::Cancelled;

        case S::PostProcessing:
            return to == S::Completed || to == S::Failed || to == S::Cancelled;

        case S::Failed:
            return to == S::Queued || to == S::Cancelled;

        case S::Completed:
        case S::Cancelled:
            return false; /* terminal states — no outgoing transitions */
    }

    return false;
}

} // namespace downloader
} // namespace vlc
