/*****************************************************************************
 * temp_file_manager.hpp
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

#ifndef VLC_DOWNLOADER_TEMP_FILE_MANAGER_HPP
#define VLC_DOWNLOADER_TEMP_FILE_MANAGER_HPP

#include <string>
#include <vector>

namespace vlc {
namespace downloader {

/**
 * @brief RAII manager for temporary files and directories.
 *
 * Creates temp files/directories in a platform-appropriate temporary location.
 * All tracked files are cleaned up when the manager is destroyed.
 * Files that have been successfully moved to their final destination should
 * be released via releaseFile() to avoid unintended deletion.
 *
 * Usage:
 * @code
 *   TempFileManager tmpMgr;
 *   std::string tmpVideo = tmpMgr.createTempFile("video", ".mp4");
 *   // ... download to tmpVideo ...
 *   std::string finalPath = "/path/to/output.mp4";
 *   vlc_rename(tmpVideo.c_str(), finalPath.c_str());
 *   tmpMgr.releaseFile(tmpVideo);  // Don't delete — it was moved
 * @endcode
 */
class TempFileManager
{
public:
    /**
     * @brief Create a TempFileManager.
     *
     * @param baseDir  Optional base temp directory. If empty, uses system temp dir
     *                 (TMPDIR, TEMP, TMP, or /tmp).
     */
    explicit TempFileManager(const std::string& baseDir = "");
    ~TempFileManager();

    /** Non-copyable, movable */
    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator=(const TempFileManager&) = delete;
    TempFileManager(TempFileManager&&) noexcept;
    TempFileManager& operator=(TempFileManager&&) noexcept;

    /**
     * @brief Create a temporary file.
     *
     * @param prefix  Filename prefix (e.g., "download").
     * @param suffix  Filename suffix/extension (e.g., ".mp4").
     * @return The path to the temp file, or empty string on failure.
     */
    std::string createTempFile(const std::string& prefix = "download",
                                const std::string& suffix = "");

    /**
     * @brief Create a temporary directory.
     *
     * @param prefix  Directory name prefix.
     * @return The path to the temp directory, or empty string on failure.
     */
    std::string createTempDir(const std::string& prefix = "download");

    /**
     * @brief Register a file for cleanup.
     *
     * Useful for files created externally that should be cleaned up.
     */
    void registerFile(const std::string& path);

    /**
     * @brief Release a file from cleanup tracking.
     *
     * Call this after the temp file has been successfully moved/renamed
     * to its final destination, to prevent it from being deleted.
     */
    void releaseFile(const std::string& path);

    /**
     * @brief Remove all tracked temporary files and directories.
     *
     * Called automatically on destruction. Can be called earlier to force cleanup.
     */
    void cleanup();

    /**
     * @brief Release all tracked files without deleting them.
     *
     * Use this when all downloads completed successfully and all temp files
     * have been moved to their final locations.
     */
    void releaseAll();

    /** @brief The base temp directory being used. */
    const std::string& baseDir() const { return m_baseDir; }

private:
    std::string m_baseDir;
    std::vector<std::string> m_tempFiles;
    std::vector<std::string> m_tempDirs;
    bool m_ownsBaseDir = false;

    static std::string systemTempDir();
    std::string generateUniquePath(const std::string& prefix, const std::string& suffix);
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_TEMP_FILE_MANAGER_HPP
