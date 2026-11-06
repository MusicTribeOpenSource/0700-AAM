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
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// A small JSON reader for the files this service owns (gesture.json, landmark records, eval labels).
// It follows RFC 8259 except that it rejects nesting deeper than kMaxDepth and does not decode \u escapes
// beyond the basic multilingual plane into UTF-8 pairs (a lone surrogate becomes U+FFFD).
namespace kickgest
{
    class JsonValue
    {
    public:
        enum class Type
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object
        };

        static constexpr int kMaxDepth = 32;

        Type type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        std::string string;
        std::vector<JsonValue> array;
        std::vector<std::pair<std::string, JsonValue>> object;

        bool isNumber() const { return type == Type::Number; }
        bool isString() const { return type == Type::String; }
        bool isArray() const { return type == Type::Array; }
        bool isObject() const { return type == Type::Object; }

        // First member with this key, or nullptr (also nullptr when this is not an object).
        const JsonValue* find(const std::string& key) const;
    };

    // Parses one complete JSON document. Trailing non-space characters are an error.
    bool parseJson(const std::string& text, JsonValue& out, std::string& error);

    // Escapes a string for use between double quotes.
    std::string jsonEscape(const std::string& s);
}
