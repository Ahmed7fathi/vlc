/*****************************************************************************
 * json_parser.hpp
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

#ifndef VLC_DOWNLOADER_JSON_PARSER_HPP
#define VLC_DOWNLOADER_JSON_PARSER_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace vlc {
namespace downloader {

/**
 * @brief Minimal recursive-descent JSON value tree.
 *
 * Parses a JSON string into a navigable tree of JsonValue nodes.
 * Supports the subset of JSON produced by yt-dlp --dump-json:
 * objects, arrays, strings, numbers, booleans, and null.
 *
 * This avoids pulling in the C JSON parser from modules/demux/json/
 * which is compiled as part of a different VLC module.
 */
class JsonValue
{
public:
    enum class Type : uint8_t
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    JsonValue() = default;

    /** @brief Construct a value of a specific type. */
    explicit JsonValue(Type t) : m_type(t) {}

    Type type() const { return m_type; }

    /** @brief Return true for non-null types. */
    explicit operator bool() const { return m_type != Type::Null; }

    // ── Accessors ──────────────────────────────────────────────────────

    bool boolValue() const;
    double numberValue() const;
    int64_t intValue() const;
    const std::string& stringValue() const;

    // ── Object access ───────────────────────────────────────────────────

    /** @brief Access a child by key. Returns a Null value if not found. */
    const JsonValue& operator[](const std::string& key) const;

    /** @brief Check if a key exists in an object. */
    bool has(const std::string& key) const;

    /** @brief Get string value of a key, or nullptr if missing/not a string. */
    const char* getString(const std::string& key) const;

    /** @brief Get numeric value of a key, or NaN if missing/not a number. */
    double getNumber(const std::string& key) const;

    /** @brief Get integer value of a key, or 0 if missing/not a number. */
    int64_t getInt(const std::string& key) const;

    // ── Array access ────────────────────────────────────────────────────

    size_t size() const;
    const JsonValue& operator[](size_t index) const;

    /** @brief Iterate over array elements. */
    using Array = std::vector<JsonValue>;
    const Array& array() const;

    /** @brief Iterate over object members. */
    using Object = std::unordered_map<std::string, JsonValue>;
    const Object& object() const;

private:
    Type m_type = Type::Null;
    bool m_bool = false;
    double m_number = 0.0;
    std::string m_string;
    Array m_array;
    Object m_object;

    friend class JsonParser;

    static const JsonValue s_null;
};

/**
 * @brief Minimal recursive-descent JSON parser.
 *
 * Usage:
 * @code
 *   JsonValue root = JsonParser::parse(jsonString);
 *   if (root.type() == JsonValue::Type::Object)
 *       const char* title = root.getString("title");
 * @endcode
 */
class JsonParser
{
public:
    /**
     * @brief Parse a JSON string into a value tree.
     * @return The root value. Returns a Null value on parse error.
     */
    static JsonValue parse(const std::string& json);

private:
    JsonParser(const std::string& input);

    JsonValue parseValue();
    JsonValue parseObject();
    JsonValue parseArray();
    JsonValue parseString();
    JsonValue parseNumber();
    JsonValue parseLiteral();  // true, false, null

    void skipWhitespace();
    bool match(char c);
    char peek();
    char advance();
    bool eof();

    /** Log a parse error. Returns without throwing; caller handles gracefully. */
    void error(const char* msg);

    const std::string& m_input;
    size_t m_pos = 0;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_JSON_PARSER_HPP
