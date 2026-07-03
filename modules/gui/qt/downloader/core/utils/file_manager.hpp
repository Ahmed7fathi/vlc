/*****************************************************************************
 * file_manager.hpp
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

#ifndef VLC_DOWNLOADER_FILE_MANAGER_HPP
#define VLC_DOWNLOADER_FILE_MANAGER_HPP

#include <string>
#include <system_error>

namespace vlc {
namespace downloader {

/**
 * @brief Manages output file paths for downloaded media.
 *
 * Provides utilities for:
 * - Resolving full output paths from directory + filename template
 * - Creating parent directories (mkdir -p)
 * - Detecting and avoiding filename collisions (appending (1), (2), etc.)
 * - Checking path writability
 * - Sanitizing filenames
 */
class FileManager
{
public:
    /**
     * @brief Result of a path resolution operation.
     */
    struct Result
    {
        std::string path;       /**< Resolved output path */
        std::string error;      /**< Empty on success, error message on failure */
        bool succeeded = false; /**< Whether the operation succeeded */
    };

    /**
     * @brief Resolve a full output path by combining directory, filename, and extension.
     *
     * Creates parent directories if they do not exist.
     *
     * @param directory   Output directory (e.g., "/home/user/Downloads").
     * @param filename    Desired filename without extension (e.g., "My Video").
     * @param extension   File extension including dot (e.g., ".mp4").
     * @param avoidCollisions  If true, append (1), (2), etc. when the file exists.
     * @return Result with the resolved path.
     */
    static Result resolveOutputPath(const std::string& directory,
                                     const std::string& filename,
                                     const std::string& extension,
                                     bool avoidCollisions = true);

    /**
     * @brief Generate a unique file path by appending a number suffix.
     *
     * E.g., "video.mp4" → "video (1).mp4" → "video (2).mp4" ...
     *
     * @param path  The desired file path.
     * @return A path that does not currently exist on disk.
     */
    static std::string avoidCollisions(const std::string& path);

    /**
     * @brief Create parent directories for a given file path.
     *
     * Equivalent to `mkdir -p $(dirname path)`.
     *
     * @param path  A file path whose parent directories should be created.
     * @return True if directories were created or already existed.
     */
    static bool ensureParentDirectories(const std::string& path);

    /**
     * @brief Create a directory and all parent directories (mkdir -p).
     *
     * @param path  Directory path to create.
     * @return True if the directory exists after the call.
     */
    static bool ensureDirectory(const std::string& path);

    /**
     * @brief Check whether a path is writable (or could be created).
     *
     * @param path  File path to check.
     * @return True if the parent directory exists and is writable.
     */
    static bool isPathWritable(const std::string& path);

    /**
     * @brief Sanitize a filename by removing or replacing illegal characters.
     *
     * Replaces characters invalid on the current platform with underscores.
     *
     * @param name  Raw filename (may contain slashes, nulls, etc.).
     * @return Sanitized filename safe for the filesystem.
     */
    static std::string sanitizeFilename(const std::string& name);

    /**
     * @brief Extract the filename without extension from a full path.
     *
     * @param path  Full path (e.g., "/dir/video.mp4").
     * @return Stem (e.g., "video").
     */
    static std::string stem(const std::string& path);

    /**
     * @brief Extract the file extension from a full path.
     *
     * @param path  Full path (e.g., "/dir/video.mp4").
     * @return Extension including dot (e.g., ".mp4"), or empty string.
     */
    static std::string extension(const std::string& path);
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_FILE_MANAGER_HPP
