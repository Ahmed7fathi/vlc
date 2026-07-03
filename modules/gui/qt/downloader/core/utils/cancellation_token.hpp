/*****************************************************************************
 * cancellation_token.hpp
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

#ifndef VLC_DOWNLOADER_CANCELLATION_TOKEN_HPP
#define VLC_DOWNLOADER_CANCELLATION_TOKEN_HPP

#include <atomic>
#include <memory>

namespace vlc {
namespace downloader {

/**
 * @brief Lightweight cooperative cancellation token.
 *
 * Designed for shared ownership between a canceller (e.g., UI thread)
 * and one or more workers (e.g., download threads). Workers periodically
 * check isCancelled() and abort their operation.
 *
 * Usage:
 * @code
 *   auto token = std::make_shared<CancellationToken>();
 *   // Pass token to worker thread
 *   // From UI thread: token->cancel();
 *   // From worker: if (token->isCancelled()) return;
 * @endcode
 */
class CancellationToken
{
public:
    CancellationToken() = default;
    ~CancellationToken() = default;

    /** Non-copyable, non-movable */
    CancellationToken(const CancellationToken&) = delete;
    CancellationToken& operator=(const CancellationToken&) = delete;
    CancellationToken(CancellationToken&&) = delete;
    CancellationToken& operator=(CancellationToken&&) = delete;

    /** @brief Request cancellation. May be called from any thread. */
    void cancel() { m_cancelled.store(true); }

    /**
     * @brief Check whether cancellation has been requested.
     * @return true if cancel() was called.
     * @note Thread-safe. Call frequently from long-running operations.
     */
    bool isCancelled() const { return m_cancelled.load(); }

    /**
     * @brief Reset the token so it can be reused.
     * @note Only safe if no concurrent checkers exist.
     */
    void reset() { m_cancelled.store(false); }

private:
    std::atomic<bool> m_cancelled{false};
};

/**
 * @brief Shared ownership alias for CancellationToken.
 *
 * Typical usage: create with std::make_shared<CancellationToken>(),
 * pass the shared_ptr to both the UI controller and the worker thread.
 */
using CancellationTokenShared = std::shared_ptr<CancellationToken>;

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_CANCELLATION_TOKEN_HPP
