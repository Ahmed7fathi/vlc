/*****************************************************************************
 * provider_registry.hpp
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

#ifndef VLC_DOWNLOADER_PROVIDER_REGISTRY_HPP
#define VLC_DOWNLOADER_PROVIDER_REGISTRY_HPP

#include "interfaces/i_media_provider.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vlc {
namespace downloader {

/**
 * @brief Registry of all available IMediaProvider implementations.
 *
 * Providers are registered with a priority value. When looking up a provider
 * for a URL, the registry iterates providers in priority order (highest first)
 * and returns the first one whose canHandle() returns true.
 *
 * This design supports future dynamic plugin loading: a plugin loader would
 * simply call registerProvider() with a provider instance loaded from disk.
 */
class ProviderRegistry
{
public:
    ProviderRegistry() = default;
    ~ProviderRegistry() = default;

    /** Non-copyable, movable */
    ProviderRegistry(const ProviderRegistry&) = delete;
    ProviderRegistry& operator=(const ProviderRegistry&) = delete;
    ProviderRegistry(ProviderRegistry&&) = default;
    ProviderRegistry& operator=(ProviderRegistry&&) = default;

    /**
     * @brief Register a provider.
     *
     * @param provider  Ownership of the provider instance.
     * @param priority  Higher priority providers are checked first.
     *                  Default 0. Use negative values for fallback providers.
     */
    void registerProvider(std::unique_ptr<IMediaProvider> provider, int priority = 0);

    /**
     * @brief Find the best provider for the given URL.
     *
     * Iterates all registered providers in priority order (highest first)
     * and returns the first one that returns true from canHandle().
     *
     * @param url  The URL to analyze.
     * @return     A pointer to the matching provider, or nullptr if none match.
     */
    IMediaProvider* findProvider(const std::string& url) const;

    /**
     * @brief Get a list of all registered providers (unsorted).
     * @note Pointers remain valid for the lifetime of the registry.
     */
    std::vector<IMediaProvider*> providers() const;

    /** @brief Number of registered providers. */
    size_t count() const { return m_entries.size(); }

    /** @brief Remove all registered providers. */
    void clear();

private:
    struct Entry
    {
        std::unique_ptr<IMediaProvider> provider;
        int priority;
    };

    std::vector<Entry> m_entries;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_PROVIDER_REGISTRY_HPP
