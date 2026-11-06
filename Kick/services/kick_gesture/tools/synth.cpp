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
// kick-gesture-synth: writes synthetic landmark streams, and optionally the matching eval labels.
//
//   kick-gesture-synth demo > demo.jsonl                       every gesture once, about 20 s
//   kick-gesture-synth eval-set DIR                            small labelled set for tools/gesture-eval (clips + labels.json)
//
// The streams come from the same synthetic hand the unit tests use. They prove the pipeline and the eval tooling work end
// to end; they say nothing about how well real hands are recognised.
#include "kick_gesture/record.h"
#include "kick_gesture/synth.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace kickgest;

namespace
{
    struct Label
    {
        std::string gesture;
        uint64_t startMs;
        uint64_t endMs;
    };

    // Builds one clip at a fixed frame rate and remembers where each gesture is meant to land.
    class Clip
    {
    public:
        explicit Clip(uint32_t frameMs = 67) : step_(frameMs) {}

        void hold(const SynthPose& p, uint32_t ms)
        {
            for (uint32_t e = 0; e < ms; e += step_) push(p);
        }
        void gap(uint32_t ms)
        {
            for (uint32_t e = 0; e < ms; e += step_)
            {
                frames_.push_back(synthEmptyFrame(t_, aspect_));
                t_ += step_;
            }
        }
        void sweep(SynthPose p, float x0, float x1, uint32_t ms)
        {
            const uint32_t n = ms / step_ > 0 ? ms / step_ : 1;
            for (uint32_t i = 0; i <= n; ++i)
            {
                p.x = x0 + (x1 - x0) * static_cast<float>(i) / static_cast<float>(n);
                push(p);
            }
        }
        void label(const std::string& gesture, uint64_t startMs, uint64_t endMs) { labels_.push_back({gesture, startMs, endMs}); }
        uint64_t now() const { return t_; }
        const std::vector<LandmarkFrame>& frames() const { return frames_; }
        const std::vector<Label>& labels() const { return labels_; }

    private:
        void push(const SynthPose& p)
        {
            LandmarkFrame f = synthFrame(t_, p, aspect_, true);
            f.quantize();
            frames_.push_back(f);
            t_ += step_;
        }
        uint32_t step_;
        uint64_t t_ = 10000;
        float aspect_ = 4.f / 3.f;
        std::vector<LandmarkFrame> frames_;
        std::vector<Label> labels_;
    };

    void swipe(Clip& c, bool right)
    {
        const float x0 = right ? 0.2f : 0.8f;
        const float x1 = right ? 0.8f : 0.2f;
        // The hand enters, swipes and leaves. Resting an open palm would be a palm_hold, which is correct behaviour but not what these clips test.
        c.hold(synthOpenPalm(x0, 0.5f), 200);
        const uint64_t t0 = c.now();
        c.sweep(synthOpenPalm(x0, 0.5f), x0, x1, 350);
        c.label(right ? "swipe_right" : "swipe_left", t0, c.now());
        c.gap(1500);
    }

    void palmHold(Clip& c)
    {
        const uint64_t t0 = c.now();
        c.hold(synthOpenPalm(0.5f, 0.5f), 1500);
        c.label("palm_hold", t0 + 500, t0 + 1000);
        c.gap(800);
    }

    void fist(Clip& c)
    {
        const uint64_t t0 = c.now();
        c.hold(synthFist(0.5f, 0.5f), 1200);
        c.label("fist", t0 + 300, t0 + 800);
        c.gap(800);
    }

    void pinch(Clip& c)
    {
        c.hold(synthOpenPalm(0.5f, 0.5f), 300);
        const uint64_t t0 = c.now();
        c.hold(synthPinch(0.5f, 0.5f, 0.2f), 1500);
        c.label("pinch_start", t0, t0 + 400);
        const uint64_t t1 = c.now();
        c.hold(synthOpenPalm(0.5f, 0.5f), 300);
        c.label("pinch_end", t1, t1 + 400);
        c.gap(800);
    }

    uint32_t rngState = 7;
    float rnd()
    {
        rngState = rngState * 1664525u + 1013904223u;
        return static_cast<float>(rngState >> 8) / static_cast<float>(1u << 24) - 0.5f;
    }

    // Small fast wiggles with a moving hand shape, the way hands move during speech.
    void talking(Clip& c, uint32_t ms)
    {
        SynthPose p = synthOpenPalm(0.5f, 0.5f);
        for (uint32_t e = 0; e < ms; e += 67)
        {
            const float ph = 6.2831853f * 2.f * static_cast<float>(e) / 1000.f;
            p.x = 0.5f + 0.054f * std::sin(ph) + 0.004f * rnd();
            p.y = 0.5f + 0.05f * std::sin(ph * 0.5f) + 0.004f * rnd();
            p.curl[2] = 0.5f + 0.4f * std::sin(ph * 0.7f);
            p.curl[3] = 0.4f + 0.4f * std::sin(ph * 1.3f);
            c.hold(p, 67);
        }
    }

    // A hand rising from the lower right toward a knob, closing as it goes.
    void reach(Clip& c)
    {
        SynthPose p = synthOpenPalm(0.7f, 0.9f);
        for (int i = 0; i <= 12; ++i)
        {
            const float u = static_cast<float>(i) / 12.f;
            p.x = 0.7f - 0.05f * u;
            p.y = 0.9f - 0.5f * u;
            for (float& k : p.curl) k = 0.6f * u;
            c.hold(p, 67);
        }
        c.gap(1500);
    }

    void writeClip(const std::string& path, const Clip& c)
    {
        std::ofstream out(path);
        LandmarkWriter w(out);
        w.writeHeader();
        for (const LandmarkFrame& f : c.frames()) w.write(f);
    }

    int usage()
    {
        std::fputs("usage: kick-gesture-synth demo > out.jsonl\n       kick-gesture-synth eval-set DIR\n", stderr);
        return 2;
    }
}

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "demo") == 0)
    {
        Clip c;
        palmHold(c);
        swipe(c, true);
        swipe(c, false);
        fist(c);
        pinch(c);
        LandmarkWriter w(std::cout);
        w.writeHeader();
        for (const LandmarkFrame& f : c.frames()) w.write(f);
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "eval-set") == 0)
    {
        const std::string dir = argv[2];
        ::mkdir(dir.c_str(), 0755);
        ::mkdir((dir + "/clips").c_str(), 0755);
        struct Entry
        {
            std::string name;
            Clip clip;
        };
        std::vector<Entry> entries;
        auto add = [&](const std::string& name, auto fill, int repeat) {
            Clip c;
            for (int i = 0; i < repeat; ++i) fill(c);
            entries.push_back({name, std::move(c)});
        };
        add("swipe_right", [](Clip& c) { swipe(c, true); }, 5);
        add("swipe_left", [](Clip& c) { swipe(c, false); }, 5);
        add("palm_hold", [](Clip& c) { palmHold(c); }, 5);
        add("fist", [](Clip& c) { fist(c); }, 5);
        add("pinch", [](Clip& c) { pinch(c); }, 5);
        add("neg_talking", [](Clip& c) { talking(c, 120000); }, 5); // ten minutes
        add("neg_reach", [](Clip& c) { reach(c); }, 20);
        std::ofstream labels(dir + "/labels.json");
        labels << "{\n \"version\": 1,\n \"clips\": [\n";
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const Entry& e = entries[i];
            writeClip(dir + "/clips/" + e.name + ".jsonl", e.clip);
            labels << "  {\"file\": \"clips/" << e.name << ".jsonl\", \"labels\": [";
            for (size_t k = 0; k < e.clip.labels().size(); ++k)
            {
                const Label& l = e.clip.labels()[k];
                labels << (k ? ", " : "") << "{\"gesture\": \"" << l.gesture << "\", \"start_ms\": " << l.startMs << ", \"end_ms\": " << l.endMs << "}";
            }
            labels << "]}" << (i + 1 < entries.size() ? "," : "") << "\n";
        }
        labels << " ]\n}\n";
        return 0;
    }
    return usage();
}
