/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "kick_gesture/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace kickgest
{
    const JsonValue* JsonValue::find(const std::string& key) const
    {
        if (type != Type::Object) return nullptr;
        for (const auto& kv : object)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }

    namespace
    {
        class Parser
        {
        public:
            Parser(const std::string& t, std::string& e) : text_(t), err_(e) {}

            bool parseDocument(JsonValue& out)
            {
                skipSpace();
                if (!parseValue(out, 0)) return false;
                skipSpace();
                if (pos_ != text_.size()) return fail("trailing characters after the document");
                return true;
            }

        private:
            const std::string& text_;
            std::string& err_;
            size_t pos_ = 0;

            bool fail(const std::string& what)
            {
                err_ = what + " at offset " + std::to_string(pos_);
                return false;
            }

            void skipSpace()
            {
                while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
                    ++pos_;
            }

            bool literal(const char* word)
            {
                size_t i = 0;
                while (word[i] != 0)
                {
                    if (pos_ + i >= text_.size() || text_[pos_ + i] != word[i]) return false;
                    ++i;
                }
                pos_ += i;
                return true;
            }

            bool parseValue(JsonValue& v, int depth)
            {
                if (depth > JsonValue::kMaxDepth) return fail("nesting too deep");
                if (pos_ >= text_.size()) return fail("unexpected end of input");
                const char c = text_[pos_];
                if (c == '{') return parseObject(v, depth);
                if (c == '[') return parseArray(v, depth);
                if (c == '"')
                {
                    v.type = JsonValue::Type::String;
                    return parseString(v.string);
                }
                if (c == 't')
                {
                    if (!literal("true")) return fail("bad literal");
                    v.type = JsonValue::Type::Bool;
                    v.boolean = true;
                    return true;
                }
                if (c == 'f')
                {
                    if (!literal("false")) return fail("bad literal");
                    v.type = JsonValue::Type::Bool;
                    v.boolean = false;
                    return true;
                }
                if (c == 'n')
                {
                    if (!literal("null")) return fail("bad literal");
                    v.type = JsonValue::Type::Null;
                    return true;
                }
                return parseNumber(v);
            }

            bool parseNumber(JsonValue& v)
            {
                const size_t start = pos_;
                if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
                if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return fail("bad number");
                if (text_[pos_] == '0')
                    ++pos_;
                else
                    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
                if (pos_ < text_.size() && text_[pos_] == '.')
                {
                    ++pos_;
                    if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return fail("bad number");
                    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
                }
                if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
                {
                    ++pos_;
                    if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
                    if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return fail("bad number");
                    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
                }
                const double d = std::strtod(text_.substr(start, pos_ - start).c_str(), nullptr);
                if (!std::isfinite(d)) return fail("number out of range");
                v.type = JsonValue::Type::Number;
                v.number = d;
                return true;
            }

            static void appendUtf8(std::string& s, unsigned cp)
            {
                if (cp < 0x80)
                    s += static_cast<char>(cp);
                else if (cp < 0x800)
                {
                    s += static_cast<char>(0xC0 | (cp >> 6));
                    s += static_cast<char>(0x80 | (cp & 0x3F));
                }
                else
                {
                    s += static_cast<char>(0xE0 | (cp >> 12));
                    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    s += static_cast<char>(0x80 | (cp & 0x3F));
                }
            }

            bool parseString(std::string& out)
            {
                out.clear();
                ++pos_; // opening quote
                while (true)
                {
                    if (pos_ >= text_.size()) return fail("unterminated string");
                    const unsigned char c = static_cast<unsigned char>(text_[pos_++]);
                    if (c == '"') return true;
                    if (c < 0x20) return fail("control character in string");
                    if (c != '\\')
                    {
                        out += static_cast<char>(c);
                        continue;
                    }
                    if (pos_ >= text_.size()) return fail("unterminated escape");
                    const char e = text_[pos_++];
                    switch (e)
                    {
                        case '"': out += '"'; break;
                        case '\\': out += '\\'; break;
                        case '/': out += '/'; break;
                        case 'b': out += '\b'; break;
                        case 'f': out += '\f'; break;
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;
                        case 'u':
                        {
                            if (pos_ + 4 > text_.size()) return fail("short \\u escape");
                            unsigned cp = 0;
                            for (int i = 0; i < 4; ++i)
                            {
                                const char h = text_[pos_++];
                                cp <<= 4;
                                if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                                else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                                else return fail("bad \\u escape");
                            }
                            if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
                            appendUtf8(out, cp);
                            break;
                        }
                        default: return fail("bad escape");
                    }
                }
            }

            bool parseArray(JsonValue& v, int depth)
            {
                v.type = JsonValue::Type::Array;
                ++pos_;
                skipSpace();
                if (pos_ < text_.size() && text_[pos_] == ']')
                {
                    ++pos_;
                    return true;
                }
                while (true)
                {
                    v.array.emplace_back();
                    skipSpace();
                    if (!parseValue(v.array.back(), depth + 1)) return false;
                    skipSpace();
                    if (pos_ >= text_.size()) return fail("unterminated array");
                    if (text_[pos_] == ',')
                    {
                        ++pos_;
                        continue;
                    }
                    if (text_[pos_] == ']')
                    {
                        ++pos_;
                        return true;
                    }
                    return fail("expected ',' or ']'");
                }
            }

            bool parseObject(JsonValue& v, int depth)
            {
                v.type = JsonValue::Type::Object;
                ++pos_;
                skipSpace();
                if (pos_ < text_.size() && text_[pos_] == '}')
                {
                    ++pos_;
                    return true;
                }
                while (true)
                {
                    skipSpace();
                    if (pos_ >= text_.size() || text_[pos_] != '"') return fail("expected a string key");
                    std::string key;
                    if (!parseString(key)) return false;
                    skipSpace();
                    if (pos_ >= text_.size() || text_[pos_] != ':') return fail("expected ':'");
                    ++pos_;
                    skipSpace();
                    v.object.emplace_back(std::move(key), JsonValue());
                    if (!parseValue(v.object.back().second, depth + 1)) return false;
                    skipSpace();
                    if (pos_ >= text_.size()) return fail("unterminated object");
                    if (text_[pos_] == ',')
                    {
                        ++pos_;
                        continue;
                    }
                    if (text_[pos_] == '}')
                    {
                        ++pos_;
                        return true;
                    }
                    return fail("expected ',' or '}'");
                }
            }
        };
    }

    bool parseJson(const std::string& text, JsonValue& out, std::string& error)
    {
        out = JsonValue();
        Parser p(text, error);
        if (!p.parseDocument(out))
        {
            out = JsonValue();
            return false;
        }
        return true;
    }

    std::string jsonEscape(const std::string& s)
    {
        std::string r;
        r.reserve(s.size() + 2);
        for (const char ch : s)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            switch (c)
            {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        r += buf;
                    }
                    else
                        r += static_cast<char>(c);
            }
        }
        return r;
    }
}
