/*****************************************************************************
 * file_manager.cpp
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

#include "file_manager.hpp"

#include <vlc_common.h>
#include <vlc_fs.h>

#include <sys/stat.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>

namespace vlc {
namespace downloader {

/* static */
FileManager::Result FileManager::resolveOutputPath(const std::string& directory,
                                                    const std::string& filename,
                                                    const std::string& ext,
                                                    bool avoidCollisionCheck)
{
    Result result;

    /* Sanitize the filename */
    std::string safeName = sanitizeFilename(filename);
    if (safeName.empty())
        safeName = "download";

    /* Build path: directory / filename.extension */
    std::string path;
    if (!directory.empty())
    {
        path = directory;
        if (path.back() != '/')
            path += '/';
    }
    path += safeName + ext;

    /* Check for collisions */
    if (avoidCollisionCheck)
        path = avoidCollisions(path);

    /* Ensure parent directories exist */
    if (!ensureParentDirectories(path))
    {
        result.error = "Failed to create parent directories";
        return result;
    }

    result.path = std::move(path);
    result.succeeded = true;
    return result;
}

/* static */
std::string FileManager::avoidCollisions(const std::string& path)
{
    struct stat st;
    if (vlc_stat(path.c_str(), &st) != 0)
        return path; /* File doesn't exist yet */

    std::string base = stem(path);
    std::string ext = extension(path);

    /* Try "filename (1).ext", "filename (2).ext", etc. */
    for (int counter = 1; counter < 10000; ++counter)
    {
        std::string candidate = base + " (" + std::to_string(counter) + ")" + ext;
        if (vlc_stat(candidate.c_str(), &st) != 0)
            return candidate;
    }

    /* Fallback: append timestamp */
    return base + " (" + std::to_string(static_cast<long long>(
        std::chrono::system_clock::now().time_since_epoch().count())) + ")" + ext;
}

/* static */
bool FileManager::ensureParentDirectories(const std::string& path)
{
    /* Extract parent directory from path */
    size_t pos = path.rfind('/');
    if (pos == std::string::npos)
        return true; /* No directory component, assume CWD */

    std::string dir = path.substr(0, pos);
    if (dir.empty())
        return true; /* Root directory */

    return ensureDirectory(dir);
}

/* static */
bool FileManager::ensureDirectory(const std::string& path)
{
    if (vlc_mkdir_parent(path.c_str(), 0755) != 0)
    {
        /* EEXIST is OK — directory already exists */
        if (errno != EEXIST)
            return false;
    }
    return true;
}

/* static */
bool FileManager::isPathWritable(const std::string& path)
{
    /* Extract parent directory */
    size_t pos = path.rfind('/');
    if (pos == std::string::npos)
        return true; /* Relative path in CWD, assume writable */

    std::string dir = path.substr(0, pos);
    if (dir.empty())
        dir = "/";

    struct stat st;
    if (vlc_stat(dir.c_str(), &st) != 0)
        return false;

    /* Check write permission */
    return (st.st_mode & S_IWUSR) != 0;
}

/* static */
std::string FileManager::sanitizeFilename(const std::string& name)
{
    if (name.empty())
        return name;

    std::string result;
    result.reserve(name.size());

    for (char c : name)
    {
        /* Replace characters illegal on most filesystems */
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == '\0')
        {
            result += '_';
        }
        else
        {
            result += c;
        }
    }

    /* Trim trailing dots and spaces (illegal on Windows) */
    while (!result.empty() && (result.back() == '.' || result.back() == ' '))
        result.pop_back();

    if (result.empty())
        result = "download";

    return result;
}

/* static */
std::string FileManager::stem(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos || dot == 0)
        return path;

    return path.substr(0, dot);
}

/* static */
std::string FileManager::extension(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos || dot == 0)
        return {};

    return path.substr(dot);
}

} // namespace downloader
} // namespace vlc
