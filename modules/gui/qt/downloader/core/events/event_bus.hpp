/*****************************************************************************
 * event_bus.hpp
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

#ifndef VLC_DOWNLOADER_EVENT_BUS_HPP
#define VLC_DOWNLOADER_EVENT_BUS_HPP

#include <memory>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <mutex>
#include <algorithm>

namespace vlc {
namespace downloader {

/**
 * @brief Type-safe heterogeneous event bus.
 *
 * Components communicate by publishing and subscribing to events.
 * The bus is type-safe: subscribers are registered for specific event types
 * and only receive events of that type.
 *
 * Event types are plain structs defined in download_events.hpp.
 *
 * Usage:
 * @code
 *   EventBus bus;
 *   bus.subscribe<TaskCreated>([](const TaskCreated& e) {
 *       // handle task created event
 *   });
 *   bus.publish(TaskCreated{task});
 * @endcode
 *
 * Thread safety: publish() and subscribe() are thread-safe via a mutex.
 * Subscribers are invoked synchronously on the publisher's thread.
 */
class EventBus
{
public:
    EventBus() = default;
    ~EventBus() = default;

    /** Non-copyable, movable */
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = default;
    EventBus& operator=(EventBus&&) = default;

    /**
     * @brief Subscribe to an event type.
     *
     * @tparam Event  The event type (must be a struct from download_events.hpp).
     * @param handler Callback invoked when an event of this type is published.
     * @return A unique subscription ID that can be used to unsubscribe.
     */
    template<typename Event>
    size_t subscribe(std::function<void(const Event&)> handler)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t id = m_nextId++;
        m_handlers[std::type_index(typeid(Event))].emplace_back(id, [h = std::move(handler)](const void* event) {
            h(*static_cast<const Event*>(event));
        });
        return id;
    }

    /**
     * @brief Publish an event to all subscribers.
     *
     * @tparam Event  The event type.
     * @param event   The event data.
     */
    template<typename Event>
    void publish(const Event& event)
    {
        std::vector<std::pair<size_t, std::function<void(const void*)>>> handlers;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_handlers.find(std::type_index(typeid(event)));
            if (it == m_handlers.end())
                return;
            handlers = it->second; /* Copy under lock */
        }
        for (const auto& [id, handler] : handlers)
            handler(&event);
    }

    /**
     * @brief Unsubscribe a handler by its subscription ID.
     *
     * @tparam Event  The event type the handler was subscribed to.
     * @param subscriptionId The ID returned by subscribe().
     */
    template<typename Event>
    void unsubscribe(size_t subscriptionId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_handlers.find(std::type_index(typeid(Event)));
        if (it == m_handlers.end())
            return;
        auto& handlers = it->second;
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                           [subscriptionId](const auto& pair) {
                               return pair.first == subscriptionId;
                           }),
            handlers.end());
    }

    /** @brief Remove all subscribers. */
    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.clear();
    }

private:
    std::mutex m_mutex;
    size_t m_nextId = 1;
    std::unordered_map<std::type_index,
                       std::vector<std::pair<size_t, std::function<void(const void*)>>>> m_handlers;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_EVENT_BUS_HPP
