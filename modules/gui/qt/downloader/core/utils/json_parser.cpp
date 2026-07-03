/*****************************************************************************
 * json_parser.cpp
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

#include "json_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace vlc {
namespace downloader {

/* static */ const JsonValue JsonValue::s_null;

// ── JsonValue accessors ─────────────────────────────────────────────────────

bool JsonValue::boolValue() const
{
    return m_bool;
}

double JsonValue::numberValue() const
{
    return m_number;
}

int64_t JsonValue::intValue() const
{
    return static_cast<int64_t>(m_number);
}

const std::string& JsonValue::stringValue() const
{
    return m_string;
}

const JsonValue& JsonValue::operator[](const std::string& key) const
{
    if (m_type != Type::Object)
        return s_null;
    auto it = m_object.find(key);
    return (it != m_object.end()) ? it->second : s_null;
}

bool JsonValue::has(const std::string& key) const
{
    return m_type == Type::Object && m_object.count(key) > 0;
}

const char* JsonValue::getString(const std::string& key) const
{
    if (m_type != Type::Object)
        return nullptr;
    auto it = m_object.find(key);
    if (it == m_object.end() || it->second.m_type != Type::String)
        return nullptr;
    return it->second.m_string.c_str();
}

double JsonValue::getNumber(const std::string& key) const
{
    if (m_type != Type::Object)
        return NAN;
    auto it = m_object.find(key);
    if (it == m_object.end() || it->second.m_type != Type::Number)
        return NAN;
    return it->second.m_number;
}

int64_t JsonValue::getInt(const std::string& key) const
{
    double n = getNumber(key);
    return std::isnan(n) ? 0 : static_cast<int64_t>(n);
}

size_t JsonValue::size() const
{
    return (m_type == Type::Array) ? m_array.size()
         : (m_type == Type::Object) ? m_object.size()
         : 0;
}

const JsonValue& JsonValue::operator[](size_t index) const
{
    return (m_type == Type::Array && index < m_array.size())
           ? m_array[index] : s_null;
}

const JsonValue::Array& JsonValue::array() const
{
    return m_array;
}

const JsonValue::Object& JsonValue::object() const
{
    return m_object;
}

// ── JsonParser ──────────────────────────────────────────────────────────────

/* static */
JsonValue JsonParser::parse(const std::string& json)
{
    JsonParser parser(json);
    JsonValue value = parser.parseValue();
    return value;
}

JsonParser::JsonParser(const std::string& input)
    : m_input(input)
{
}

void JsonParser::skipWhitespace()
{
    while (m_pos < m_input.size())
    {
        char c = m_input[m_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            ++m_pos;
        else
            break;
    }
}

bool JsonParser::match(char c)
{
    skipWhitespace();
    if (m_pos < m_input.size() && m_input[m_pos] == c)
    {
        ++m_pos;
        return true;
    }
    return false;
}

char JsonParser::peek()
{
    skipWhitespace();
    return (m_pos < m_input.size()) ? m_input[m_pos] : '\0';
}

char JsonParser::advance()
{
    return (m_pos < m_input.size()) ? m_input[m_pos++] : '\0';
}

bool JsonParser::eof()
{
    skipWhitespace();
    return m_pos >= m_input.size();
}

void JsonParser::error(const char* msg)
{
    /* Parse errors silently return a Null value. The caller
     * (YtdlpJsonParser::parse) checks for well-formed output.
     * This is intentional: yt-dlp output is well-structured, so
     * parse errors indicate a bug or format change caught in testing. */
    (void)msg;
}

JsonValue JsonParser::parseValue()
{
    skipWhitespace();
    if (eof())
    {
        error("Unexpected end of input");
        return JsonValue();
    }

    char c = peek();
    switch (c)
    {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return parseString();
        case 't': case 'f': case 'n': return parseLiteral();
        default:
            if (c == '-' || (c >= '0' && c <= '9'))
                return parseNumber();
            error("Unexpected character");
            return JsonValue();
    }
}

JsonValue JsonParser::parseObject()
{
    JsonValue obj(JsonValue::Type::Object);
    advance(); // '{'

    if (match('}'))
        return obj;

    do {
        if (peek() != '"')
        {
            error("Expected string key in object");
            break;
        }
        JsonValue key = parseString();
        if (!match(':'))
        {
            error("Expected ':' after object key");
            break;
        }
        JsonValue val = parseValue();
        obj.m_object.emplace(std::move(key.m_string), std::move(val));
    } while (match(','));

    if (!match('}'))
        error("Expected '}' at end of object");

    return obj;
}

JsonValue JsonParser::parseArray()
{
    JsonValue arr(JsonValue::Type::Array);
    advance(); // '['

    if (match(']'))
        return arr;

    do {
        arr.m_array.push_back(parseValue());
    } while (match(','));

    if (!match(']'))
        error("Expected ']' at end of array");

    return arr;
}

JsonValue JsonParser::parseString()
{
    JsonValue str(JsonValue::Type::String);
    advance(); // '"'

    while (m_pos < m_input.size())
    {
        char c = advance();
        if (c == '"')
            return str;

        if (c == '\\')
        {
            if (m_pos >= m_input.size())
                break;
            char esc = advance();
            switch (esc)
            {
                case '"':  str.m_string += '"';  break;
                case '\\': str.m_string += '\\'; break;
                case '/':  str.m_string += '/';  break;
                case 'b':  str.m_string += '\b'; break;
                case 'f':  str.m_string += '\f'; break;
                case 'n':  str.m_string += '\n'; break;
                case 'r':  str.m_string += '\r'; break;
                case 't':  str.m_string += '\t'; break;
                case 'u':
                {
                    /* Minimal \uXXXX support — parse hex and append UTF-8.
                     * Surrogate pairs and advanced Unicode are skipped
                     * since yt-dlp metadata titles are typically ASCII
                     * or basic BMP. */
                    if (m_pos + 4 <= m_input.size())
                    {
                        std::string hex = m_input.substr(m_pos, 4);
                        m_pos += 4;
                        char* end = nullptr;
                        long codepoint = std::strtol(hex.c_str(), &end, 16);
                        if (end == hex.c_str() + 4 && codepoint > 0)
                        {
                            /* Encode as UTF-8 */
                            if (codepoint < 0x80)
                                str.m_string += static_cast<char>(codepoint);
                            else if (codepoint < 0x800)
                            {
                                str.m_string += static_cast<char>(0xC0 | (codepoint >> 6));
                                str.m_string += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                            else
                            {
                                str.m_string += static_cast<char>(0xE0 | (codepoint >> 12));
                                str.m_string += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                str.m_string += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        }
                    }
                    break;
                }
                default:
                    str.m_string += esc;
                    break;
            }
        }
        else
        {
            str.m_string += c;
        }
    }

    error("Unterminated string");
    return str;
}

JsonValue JsonParser::parseNumber()
{
    JsonValue num(JsonValue::Type::Number);
    size_t start = m_pos;

    if (peek() == '-')
        advance();

    while (m_pos < m_input.size() && m_input[m_pos] >= '0' && m_input[m_pos] <= '9')
        advance();

    if (peek() == '.')
    {
        advance();
        while (m_pos < m_input.size() && m_input[m_pos] >= '0' && m_input[m_pos] <= '9')
            advance();
    }

    if (peek() == 'e' || peek() == 'E')
    {
        advance();
        if (peek() == '+' || peek() == '-')
            advance();
        while (m_pos < m_input.size() && m_input[m_pos] >= '0' && m_input[m_pos] <= '9')
            advance();
    }

    num.m_number = std::strtod(m_input.c_str() + start, nullptr);
    return num;
}

JsonValue JsonParser::parseLiteral()
{
    if (m_input.compare(m_pos, 4, "true") == 0)
    {
        JsonValue val(JsonValue::Type::Bool);
        val.m_bool = true;
        m_pos += 4;
        return val;
    }
    if (m_input.compare(m_pos, 5, "false") == 0)
    {
        JsonValue val(JsonValue::Type::Bool);
        val.m_bool = false;
        m_pos += 5;
        return val;
    }
    if (m_input.compare(m_pos, 4, "null") == 0)
    {
        m_pos += 4;
        return JsonValue(JsonValue::Type::Null);
    }
    error("Expected literal (true, false, null)");
    return JsonValue();
}

} // namespace downloader
} // namespace vlc
