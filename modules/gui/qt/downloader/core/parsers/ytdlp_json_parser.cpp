/*****************************************************************************
 * ytdlp_json_parser.cpp
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

#include "ytdlp_json_parser.hpp"
#include "../utils/json_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace vlc {
namespace downloader {

/* static */
std::unique_ptr<MediaInfo> YtdlpJsonParser::parse(const std::string& jsonText)
{
    JsonValue root = JsonParser::parse(jsonText);
    if (root.type() != JsonValue::Type::Object)
        return nullptr;

    auto info = std::make_unique<MediaInfo>();

    /* ── Basic metadata ─────────────────────────────────────────────── */

    if (const char* s = root.getString("title"))
        info->title = s;
    if (const char* s = root.getString("uploader"))
        info->uploader = s;
    if (const char* s = root.getString("description"))
        info->description = s;
    if (const char* s = root.getString("upload_date"))
        info->uploadDate = s;
    if (const char* s = root.getString("webpage_url"))
        info->webpageUrl = s;
    if (const char* s = root.getString("extractor"))
        info->extractor = s;
    if (const char* s = root.getString("original_url"))
        info->url = s;
    if (info->url.empty())
    {
        if (const char* s = root.getString("webpage_url"))
            info->url = s;
    }

    /* Duration (in seconds) */
    double duration = root.getNumber("duration");
    if (!std::isnan(duration) && duration > 0)
        info->duration = static_cast<int64_t>(duration);

    /* View count */
    int64_t views = root.getInt("view_count");
    if (views > 0)
        info->viewCount = views;

    /* ── Thumbnail ──────────────────────────────────────────────────── */

    /* Prefer the last (highest resolution) thumbnail */
    const JsonValue& thumbnails = root["thumbnails"];
    if (thumbnails.type() == JsonValue::Type::Array && thumbnails.size() > 0)
    {
        const JsonValue& best = thumbnails[thumbnails.size() - 1];
        if (const char* url = best.getString("url"))
            info->thumbnailUrl = url;
    }

    /* Fallback to single "thumbnail" field */
    if (info->thumbnailUrl.empty())
    {
        if (const char* s = root.getString("thumbnail"))
            info->thumbnailUrl = s;
    }

    /* ── Video formats ─────────────────────────────────────────────────── */

    const JsonValue& formats = root["formats"];
    if (formats.type() == JsonValue::Type::Array)
    {
        for (size_t i = 0; i < formats.size(); ++i)
        {
            const JsonValue& fmt = formats[i];
            if (fmt.type() != JsonValue::Type::Object)
                continue;

            VideoFormat vf;
            if (const char* s = fmt.getString("format_id"))
                vf.id = s;
            if (const char* s = fmt.getString("vcodec"))
                vf.codec = s;
            if (const char* s = fmt.getString("ext"))
                vf.extension = s;

            vf.height = static_cast<int>(fmt.getInt("height"));
            vf.width = static_cast<int>(fmt.getInt("width"));
            vf.bitrate = fmt.getInt("tbr");
            vf.filesize = fmt.getInt("filesize");
            vf.hasAudio = (fmt.getInt("acodec") != 0);
            vf.hasVideo = (vf.height > 0 && vf.codec != "none");

            /* Only include formats that have a video component */
            if (vf.hasVideo)
                info->videoFormats.push_back(std::move(vf));
        }
    }

    /* ── Audio formats (from requested_formats or the request_formats array) ── */

    /* Also look at individual formats for audio-only entries */
    if (formats.type() == JsonValue::Type::Array)
    {
        for (size_t i = 0; i < formats.size(); ++i)
        {
            const JsonValue& fmt = formats[i];
            if (fmt.type() != JsonValue::Type::Object)
                continue;

            AudioFormat af;
            if (const char* s = fmt.getString("format_id"))
                af.id = s;
            if (const char* s = fmt.getString("acodec"))
                af.codec = s;
            if (const char* s = fmt.getString("ext"))
                af.extension = s;
            af.bitrate = static_cast<int>(fmt.getInt("abr"));
            af.sampleRate = static_cast<int>(fmt.getInt("asr"));

            /* Only include audio-only or combined formats */
            int height = static_cast<int>(fmt.getInt("height"));
            if (height == 0 && !af.codec.empty() && af.codec != "none")
            {
                /* Deduplicate by id */
                bool duplicate = false;
                for (const auto& existing : info->audioFormats)
                {
                    if (existing.id == af.id)
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    info->audioFormats.push_back(std::move(af));
            }
        }
    }

    /* ── Subtitles ─────────────────────────────────────────────────────── */

    /* Manual subtitles */
    const JsonValue& subtitles = root["subtitles"];
    if (subtitles.type() == JsonValue::Type::Object)
    {
        for (const auto& [lang, subEntry] : subtitles.object())
        {
            SubtitleTrack st;
            st.id = lang;
            st.language = lang;
            st.isAutomatic = false;

            /* Try to get a human-readable name */
            if (subEntry.type() == JsonValue::Type::Array && subEntry.size() > 0)
            {
                const JsonValue& first = subEntry[0];
                if (const char* s = first.getString("name"))
                    st.name = s;
            }
            if (st.name.empty())
                st.name = lang;

            info->subtitles.push_back(std::move(st));
        }
    }

    /* Automatic captions */
    const JsonValue& autoCaptions = root["automatic_captions"];
    if (autoCaptions.type() == JsonValue::Type::Object)
    {
        for (const auto& [lang, capEntry] : autoCaptions.object())
        {
            /* Skip if manual subtitle already exists for this language */
            bool existing = false;
            for (const auto& sub : info->subtitles)
            {
                if (sub.language == lang)
                {
                    existing = true;
                    break;
                }
            }
            if (existing)
                continue;

            SubtitleTrack st;
            st.id = lang;
            st.language = lang;
            st.isAutomatic = true;

            if (capEntry.type() == JsonValue::Type::Array && capEntry.size() > 0)
            {
                const JsonValue& first = capEntry[0];
                if (const char* s = first.getString("name"))
                    st.name = s;
            }
            if (st.name.empty())
                st.name = lang + " (auto)";

            info->subtitles.push_back(std::move(st));
        }
    }

    /* ── Chapters ──────────────────────────────────────────────────────── */

    const JsonValue& chapters = root["chapters"];
    if (chapters.type() == JsonValue::Type::Array)
    {
        for (size_t i = 0; i < chapters.size(); ++i)
        {
            const JsonValue& ch = chapters[i];
            if (ch.type() != JsonValue::Type::Object)
                continue;

            Chapter c;
            if (const char* s = ch.getString("title"))
                c.title = s;
            c.startTime = ch.getInt("start_time");
            c.endTime = ch.getInt("end_time");

            info->chapters.push_back(std::move(c));
        }
    }

    return info;
}

} // namespace downloader
} // namespace vlc
