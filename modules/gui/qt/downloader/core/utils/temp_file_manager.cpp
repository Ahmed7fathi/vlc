/*****************************************************************************
 * temp_file_manager.cpp
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

#include "temp_file_manager.hpp"
#include "file_manager.hpp"

#include <vlc_common.h>
#include <vlc_fs.h>

#include <fcntl.h>

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>

namespace vlc {
namespace downloader {

namespace {

/** Generate a random hex string suffix for uniqueness. */
static std::string randomSuffix()
{
    static const char hex[] = "0123456789abcdef";
    std::string suffix(8, '0');
    /* Use time + pid for deterministic uniqueness */
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    unsigned int seed = static_cast<unsigned int>(ms & 0xFFFFFFFF);
    for (auto& c : suffix)
    {
        seed = seed * 1103515245 + 12345;
        c = hex[(seed >> 16) & 0x0F];
    }
    return suffix;
}

} // anonymous namespace

TempFileManager::TempFileManager(const std::string& baseDir)
{
    if (!baseDir.empty())
    {
        m_baseDir = baseDir;
        if (!FileManager::ensureDirectory(m_baseDir))
        {
            msg_Warn(nullptr, "Failed to create temp base dir: %s", m_baseDir.c_str());
        }
    }
    else
    {
        m_baseDir = systemTempDir();
        /* Create a unique subdirectory to avoid collisions */
        std::string subdir = m_baseDir + "/vlc-download-" + randomSuffix();
        if (FileManager::ensureDirectory(subdir))
        {
            m_baseDir = subdir;
            m_ownsBaseDir = true;
        }
    }
}

TempFileManager::~TempFileManager()
{
    cleanup();
}

TempFileManager::TempFileManager(TempFileManager&& other) noexcept
    : m_baseDir(std::move(other.m_baseDir))
    , m_tempFiles(std::move(other.m_tempFiles))
    , m_tempDirs(std::move(other.m_tempDirs))
    , m_ownsBaseDir(other.m_ownsBaseDir)
{
    other.m_ownsBaseDir = false;
    other.m_tempFiles.clear();
    other.m_tempDirs.clear();
}

TempFileManager& TempFileManager::operator=(TempFileManager&& other) noexcept
{
    if (this != &other)
    {
        cleanup();
        m_baseDir = std::move(other.m_baseDir);
        m_tempFiles = std::move(other.m_tempFiles);
        m_tempDirs = std::move(other.m_tempDirs);
        m_ownsBaseDir = other.m_ownsBaseDir;
        other.m_ownsBaseDir = false;
        other.m_tempFiles.clear();
        other.m_tempDirs.clear();
    }
    return *this;
}

std::string TempFileManager::createTempFile(const std::string& prefix,
                                             const std::string& suffix)
{
    std::string path = generateUniquePath(prefix, suffix);

    /* Create the file exclusively to verify the path is unique and writable,
     * then immediately remove it so yt-dlp (or any downloader) writes fresh
     * content. If we leave the empty file, yt-dlp's "already downloaded"
     * check will see it exists and skip the actual download, resulting in
     * a 0-byte output. */
    int fd = vlc_open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0)
    {
        msg_Err(nullptr, "Failed to create temp file '%s': %s",
                path.c_str(), vlc_strerror_c(errno));
        return {};
    }
    vlc_close(fd);
    vlc_unlink(path.c_str());

    registerFile(path);
    return path;
}

std::string TempFileManager::createTempDir(const std::string& prefix)
{
    std::string path = generateUniquePath(prefix, "");

    if (!FileManager::ensureDirectory(path))
    {
        msg_Err(nullptr, "Failed to create temp directory '%s': %s",
                path.c_str(), vlc_strerror_c(errno));
        return {};
    }

    m_tempDirs.push_back(path);
    return path;
}

void TempFileManager::registerFile(const std::string& path)
{
    if (!path.empty())
        m_tempFiles.push_back(path);
}

void TempFileManager::releaseFile(const std::string& path)
{
    auto it = std::remove(m_tempFiles.begin(), m_tempFiles.end(), path);
    m_tempFiles.erase(it, m_tempFiles.end());

    auto dit = std::remove(m_tempDirs.begin(), m_tempDirs.end(), path);
    m_tempDirs.erase(dit, m_tempDirs.end());
}

void TempFileManager::cleanup()
{
    /* Remove tracked files */
    for (const auto& file : m_tempFiles)
    {
        if (vlc_unlink(file.c_str()) != 0 && errno != ENOENT)
        {
            msg_Warn(nullptr, "Failed to remove temp file '%s': %s",
                     file.c_str(), vlc_strerror_c(errno));
        }
    }
    m_tempFiles.clear();

    /* Remove tracked directories (rmdir only works if empty) */
    for (const auto& dir : m_tempDirs)
    {
        if (vlc_unlink(dir.c_str()) != 0 && errno != ENOENT && errno != ENOTEMPTY)
        {
            msg_Warn(nullptr, "Failed to remove temp directory '%s': %s",
                     dir.c_str(), vlc_strerror_c(errno));
        }
    }
    m_tempDirs.clear();

    /* Remove our own base directory if we created it */
    if (m_ownsBaseDir && !m_baseDir.empty())
    {
        vlc_unlink(m_baseDir.c_str());
    }
}

void TempFileManager::releaseAll()
{
    m_tempFiles.clear();
    m_tempDirs.clear();
    m_ownsBaseDir = false;
}

/* static */
std::string TempFileManager::systemTempDir()
{
    const char* env = nullptr;
    if ((env = std::getenv("TMPDIR")) != nullptr) return env;
    if ((env = std::getenv("TEMP")) != nullptr)   return env;
    if ((env = std::getenv("TMP")) != nullptr)    return env;
    return "/tmp";
}

std::string TempFileManager::generateUniquePath(const std::string& prefix,
                                                  const std::string& suffix)
{
    /* Ensure base dir ends with '/' */
    std::string path = m_baseDir;
    if (path.back() != '/')
        path += '/';
    path += prefix + "-" + randomSuffix() + suffix;
    return path;
}

} // namespace downloader
} // namespace vlc
