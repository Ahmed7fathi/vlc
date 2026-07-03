/*****************************************************************************
 * ytdlp_progress_parser.cpp
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

#include "ytdlp_progress_parser.hpp"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <sstream>

namespace vlc {
namespace downloader {

namespace {

/**
 * @brief Parse a yt-dlp size string (e.g., "15.34MiB", "1.25GiB", "1024.00KiB")
 * into a byte count.
 */
static double parseSize(const char* str)
{
    if (!str || !*str)
        return 0.0;

    char* end = nullptr;
    double value = std::strtod(str, &end);
    if (end == str)
        return 0.0;

    /* Skip whitespace between number and unit */
    while (*end == ' ')
        ++end;

    if (*end == '\0')
        return value;

    if (strncmp(end, "TiB", 3) == 0) return value * 1024.0 * 1024.0 * 1024.0 * 1024.0;
    if (strncmp(end, "GiB", 3) == 0) return value * 1024.0 * 1024.0 * 1024.0;
    if (strncmp(end, "MiB", 3) == 0) return value * 1024.0 * 1024.0;
    if (strncmp(end, "KiB", 3) == 0) return value * 1024.0;
    if (strncmp(end, "TB", 2) == 0)  return value * 1000.0 * 1000.0 * 1000.0 * 1000.0;
    if (strncmp(end, "GB", 2) == 0)  return value * 1000.0 * 1000.0 * 1000.0;
    if (strncmp(end, "MB", 2) == 0)  return value * 1000.0 * 1000.0;
    if (strncmp(end, "KB", 2) == 0)  return value * 1000.0;
    if (strncmp(end, "B", 1) == 0)   return value;

    return value;
}

/**
 * @brief Parse a yt-dlp speed string (e.g., "2.34MiB/s") into bytes/sec.
 */
static double parseSpeed(const char* str)
{
    if (!str || !*str)
        return 0.0;

    /* Find the '/' that separates size from '/s' */
    const char* slash = std::strchr(str, '/');
    if (!slash)
        return 0.0;

    /* Parse the size part */
    std::string sizePart(str, slash - str);
    return parseSize(sizePart.c_str());
}

/**
 * @brief Parse a yt-dlp ETA string (e.g., "00:05", "01:23:45", "1m", "50s")
 * into seconds.
 */
static int64_t parseETA(const char* str)
{
    if (!str || !*str)
        return 0;

    /* Handle "N/A" or unknown */
    if (strcmp(str, "N/A") == 0)
        return -1;

    /* Handle "Xs" format (seconds) */
    size_t len = std::strlen(str);
    if (len > 0 && str[len - 1] == 's')
    {
        return static_cast<int64_t>(std::strtod(str, nullptr));
    }

    /* Handle "Xm" format (minutes) */
    if (len > 0 && str[len - 1] == 'm')
    {
        return static_cast<int64_t>(std::strtod(str, nullptr)) * 60;
    }

    /* Handle "HH:MM:SS" or "MM:SS" format */
    int a = 0, b = 0, c = 0;
    if (std::sscanf(str, "%d:%d:%d", &a, &b, &c) >= 3)
    {
        /* Three values: HH:MM:SS */
        return static_cast<int64_t>(a) * 3600 + static_cast<int64_t>(b) * 60 + c;
    }
    if (std::sscanf(str, "%d:%d", &a, &b) >= 2)
    {
        /* Two values: MM:SS */
        return static_cast<int64_t>(a) * 60 + b;
    }

    return 0;
}

/**
 * @brief Check if a line is a download progress line (starts with "[download] ").
 */
static bool isDownloadLine(const std::string& line)
{
    return line.find("[download]") == 0;
}

} // anonymous namespace

/* static */
std::optional<YtdlpProgressData> YtdlpProgressParser::parseLine(const std::string& line)
{
    if (!isDownloadLine(line))
        return std::nullopt;

    YtdlpProgressData data;

    /* Check for destination line: "[download] Destination: /path/to/file" */
    {
        std::string dest;
        if (parseDestination(line, dest))
        {
            data.currentFile = std::move(dest);
            return data;
        }
    }

    /* Check for "100% of X in Y" completion line */
    if (line.find("100%") != std::string::npos && line.find(" in ") != std::string::npos)
    {
        data.percent = 100;
        data.isComplete = true;

        /* Try to extract total size: "... 100% of 15.34MiB in 00:00" */
        const char* ofPos = std::strstr(line.c_str(), " of ");
        if (ofPos)
        {
            ofPos += 4;
            const char* inPos = std::strstr(ofPos, " in ");
            if (inPos)
            {
                std::string sizeStr(ofPos, inPos - ofPos);
                data.totalBytes = static_cast<int64_t>(parseSize(sizeStr.c_str()));
                data.downloadedBytes = data.totalBytes;
            }
        }
        return data;
    }

    /* Check for progress line: "  XX.X% of SIZE at SPEED ETA TIME" */
    /* Find the % sign */
    const char* pctPos = std::strchr(line.c_str(), '%');
    if (!pctPos)
        return std::nullopt;

    /* Parse percentage: scan backward from % to find the start of digits */
    const char* numStart = pctPos;
    while (numStart > line.c_str() && (isdigit(static_cast<unsigned char>(*(numStart - 1))) ||
           *(numStart - 1) == '.' || *(numStart - 1) == '-' || *(numStart - 1) == ' '))
        --numStart;
    data.percent = static_cast<int>(std::round(std::strtod(numStart, nullptr)));

    /* Find " of " */
    const char* ofPos = std::strstr(pctPos + 1, " of ");
    if (!ofPos)
        return data;

    /* Find " at " */
    const char* atPos = std::strstr(ofPos + 4, " at ");
    if (!atPos)
    {
        /* Maybe " of SIZE ETA TIME" without speed */
        const char* etaPos = std::strstr(ofPos + 4, " ETA ");
        if (etaPos)
        {
            std::string sizeStr(ofPos + 4, etaPos - (ofPos + 4));
            data.totalBytes = static_cast<int64_t>(parseSize(sizeStr.c_str()));
            data.eta = parseETA(etaPos + 5);
        }
        return data;
    }

    /* Parse total size */
    {
        std::string sizeStr(ofPos + 4, atPos - (ofPos + 4));
        data.totalBytes = static_cast<int64_t>(parseSize(sizeStr.c_str()));
    }

    /* Find " ETA " */
    const char* etaPos = std::strstr(atPos + 4, " ETA ");
    if (!etaPos)
    {
        /* Parse speed only */
        data.speed = parseSpeed(atPos + 4);
        return data;
    }

    /* Parse speed */
    {
        std::string speedStr(atPos + 4, etaPos - (atPos + 4));
        data.speed = parseSpeed(speedStr.c_str());
    }

    /* Parse ETA */
    data.eta = parseETA(etaPos + 5);

    /* Estimate downloaded bytes */
    if (data.totalBytes > 0 && data.percent > 0)
        data.downloadedBytes = static_cast<int64_t>(data.totalBytes * data.percent / 100.0);

    return data;
}

/* static */
bool YtdlpProgressParser::parseDestination(const std::string& line, std::string& path)
{
    if (!isDownloadLine(line))
        return false;

    /* Look for "Destination: " */
    const char* destPos = std::strstr(line.c_str(), "Destination: ");
    if (!destPos)
        return false;

    path = std::string(destPos + 13);  /* Skip "Destination: " */
    /* Remove trailing whitespace */
    while (!path.empty() && (path.back() == ' ' || path.back() == '\r'))
        path.pop_back();
    return true;
}

} // namespace downloader
} // namespace vlc
