/*****************************************************************************
 * provider_registry.cpp
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

#include "provider_registry.hpp"

#include <algorithm>

namespace vlc {
namespace downloader {

void ProviderRegistry::registerProvider(std::unique_ptr<IMediaProvider> provider, int priority)
{
    if (!provider)
        return;

    m_entries.push_back({std::move(provider), priority});

    /* Sort by priority descending so findProvider() can return the first match. */
    std::sort(m_entries.begin(), m_entries.end(),
              [](const Entry& a, const Entry& b) {
                  return a.priority > b.priority;
              });
}

IMediaProvider* ProviderRegistry::findProvider(const std::string& url) const
{
    for (const auto& entry : m_entries)
    {
        if (entry.provider->canHandle(url))
            return entry.provider.get();
    }
    return nullptr;
}

std::vector<IMediaProvider*> ProviderRegistry::providers() const
{
    std::vector<IMediaProvider*> result;
    result.reserve(m_entries.size());
    for (const auto& entry : m_entries)
        result.push_back(entry.provider.get());
    return result;
}

void ProviderRegistry::clear()
{
    m_entries.clear();
}

} // namespace downloader
} // namespace vlc
