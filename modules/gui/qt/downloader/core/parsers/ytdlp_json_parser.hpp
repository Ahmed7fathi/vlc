/*****************************************************************************
 * ytdlp_json_parser.hpp
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

#ifndef VLC_DOWNLOADER_YTDLP_JSON_PARSER_HPP
#define VLC_DOWNLOADER_YTDLP_JSON_PARSER_HPP

#include "../../models/media_info.hpp"

#include <memory>
#include <string>

namespace vlc {
namespace downloader {

/**
 * @brief Parses yt-dlp's --dump-json (or --dump-single-json) output into
 *        the MediaInfo domain model.
 *
 * Uses the internal JsonParser utility for JSON tree navigation.
 * Extracts all available metadata: title, uploader, duration, thumbnail,
 * formats (video + audio), subtitles, chapters, and more.
 */
class YtdlpJsonParser
{
public:
    /**
     * @brief Parse a complete yt-dlp JSON string into MediaInfo.
     *
     * @param jsonText  The raw JSON output from yt-dlp --dump-json.
     * @return A populated MediaInfo, or nullptr on parse failure.
     */
    static std::unique_ptr<MediaInfo> parse(const std::string& jsonText);
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_YTDLP_JSON_PARSER_HPP
