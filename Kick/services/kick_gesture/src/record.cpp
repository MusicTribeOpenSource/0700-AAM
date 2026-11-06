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
#include "kick_gesture/record.h"

#include "kick_gesture/json.h"

#include <cmath>
#include <cstdio>

namespace kickgest
{
    void LandmarkWriter::writeHeader()
    {
        out_ << "{\"format\":\"kick-gesture-landmarks\",\"version\":" << kRecordVersion << "}\n";
    }

    void LandmarkWriter::write(const LandmarkFrame& f)
    {
        char buf[64];
        std::string line = "{\"t_ms\":" + std::to_string(f.tMs);
        std::snprintf(buf, sizeof(buf), ",\"aspect\":%.6f,\"hands\":[", static_cast<double>(f.aspect));
        line += buf;
        for (size_t h = 0; h < f.hands.size(); ++h)
        {
            const Hand& hand = f.hands[h];
            std::snprintf(buf, sizeof(buf), "%s{\"score\":%.6f,\"right\":%.6f,\"lm\":[", h ? "," : "", static_cast<double>(hand.score), static_cast<double>(hand.rightProb));
            line += buf;
            for (size_t i = 0; i < hand.lm.size(); ++i)
            {
                std::snprintf(buf, sizeof(buf), "%s%.6f,%.6f,%.6f", i ? "," : "", static_cast<double>(hand.lm[i].x), static_cast<double>(hand.lm[i].y), static_cast<double>(hand.lm[i].z));
                line += buf;
            }
            line += "]}";
        }
        line += "]}\n";
        out_ << line;
    }

    bool LandmarkReader::next(LandmarkFrame& out)
    {
        error_.clear();
        std::string text;
        while (std::getline(in_, text))
        {
            ++line_;
            if (!text.empty() && text.back() == '\r') text.pop_back();
            bool blank = true;
            for (char c : text)
                if (c != ' ' && c != '\t') blank = false;
            if (blank) continue;

            auto fail = [&](const std::string& what) {
                error_ = "line " + std::to_string(line_) + ": " + what;
                return false;
            };
            JsonValue v;
            std::string perr;
            if (!parseJson(text, v, perr)) return fail(perr);
            if (!v.isObject()) return fail("not an object");
            if (const JsonValue* fmt = v.find("format"))
            {
                if (!fmt->isString() || fmt->string != "kick-gesture-landmarks") return fail("unknown record format");
                const JsonValue* ver = v.find("version");
                if (ver == nullptr || !ver->isNumber() || ver->number != kRecordVersion) return fail("unsupported record version");
                continue;
            }
            const JsonValue* t = v.find("t_ms");
            const JsonValue* aspect = v.find("aspect");
            const JsonValue* hands = v.find("hands");
            if (t == nullptr || !t->isNumber() || t->number < 0 || t->number != std::floor(t->number) || t->number > 9e15) return fail("t_ms must be a non-negative whole number");
            if (aspect == nullptr || !aspect->isNumber() || !(aspect->number > 0.0) || aspect->number > 100.0) return fail("aspect must be a positive number");
            if (hands == nullptr || !hands->isArray()) return fail("hands must be an array");
            LandmarkFrame f;
            f.tMs = static_cast<uint64_t>(t->number);
            f.aspect = static_cast<float>(aspect->number);
            for (const JsonValue& hv : hands->array)
            {
                const JsonValue* score = hv.find("score");
                const JsonValue* right = hv.find("right");
                const JsonValue* lm = hv.find("lm");
                if (score == nullptr || !score->isNumber() || right == nullptr || !right->isNumber() || lm == nullptr || !lm->isArray()) return fail("a hand needs score, right and lm");
                if (lm->array.size() != static_cast<size_t>(kNumLandmarks) * 3) return fail("lm must hold 63 numbers");
                Hand h;
                h.score = static_cast<float>(score->number);
                h.rightProb = static_cast<float>(right->number);
                for (int i = 0; i < kNumLandmarks; ++i)
                {
                    const JsonValue& x = lm->array[static_cast<size_t>(3 * i)];
                    const JsonValue& y = lm->array[static_cast<size_t>(3 * i + 1)];
                    const JsonValue& z = lm->array[static_cast<size_t>(3 * i + 2)];
                    if (!x.isNumber() || !y.isNumber() || !z.isNumber()) return fail("lm must hold numbers only");
                    h.lm[static_cast<size_t>(i)] = {static_cast<float>(x.number), static_cast<float>(y.number), static_cast<float>(z.number)};
                }
                f.hands.push_back(h);
            }
            out = std::move(f);
            return true;
        }
        return false;
    }
}
