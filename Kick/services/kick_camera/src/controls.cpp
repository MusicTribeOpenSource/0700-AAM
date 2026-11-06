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
#include "kick_camera/controls.h"

namespace kickcam
{
    bool parsePreset(const std::string& name, Preset& out)
    {
        if (name == "lit_room") { out = Preset::LitRoom; return true; }
        if (name == "dim_stage") { out = Preset::DimStage; return true; }
        return false;
    }

    const char* presetName(Preset p) { return p == Preset::DimStage ? "dim_stage" : "lit_room"; }

    int dimStageExposure(uint32_t fps)
    {
        const int periodUnits = static_cast<int>(10000 / (fps == 0 ? 15 : fps)); // one frame period in 100 us units
        const int exposure = periodUnits * 5 / 6;
        return exposure < 1 ? 1 : exposure;
    }

    namespace
    {
        void note(ControlReport& r, bool ok, const char* what) { (ok ? r.applied : r.unsupported).push_back(what); }

        void setExposureForPreset(FrameSource& src, Preset preset, uint32_t fps, ControlReport& r)
        {
            if (preset == Preset::DimStage)
            {
                const bool manual = src.setAutoExposure(false);
                note(r, manual, "exposure_manual");
                if (manual)
                {
                    note(r, src.setExposureAbsolute(dimStageExposure(fps)), "exposure_absolute");
                    return;
                }
                // A source that cannot go manual still gets the best it can do: auto exposure.
            }
            note(r, src.setAutoExposure(true), "exposure_auto");
        }
    }

    ControlReport applyPreset(FrameSource& src, Preset preset, uint32_t fps, ControlState& state)
    {
        ControlReport report;
        setExposureForPreset(src, preset, fps, report);
        note(report, src.setKeepFrameRate(true), "keep_frame_rate");
        note(report, src.setAutoWhiteBalance(true), "white_balance_auto");
        state.preset = preset;
        state.exposureLocked = false;
        state.wbLocked = false;
        return report;
    }

    bool lockExposure(FrameSource& src, ControlState& state, std::string& error)
    {
        int current = 0;
        const bool haveValue = src.getExposureAbsolute(current);
        if (!src.setAutoExposure(false))
        {
            error = "source cannot switch to manual exposure";
            return false;
        }
        // Writing the value back pins the exposure the sensor was using, on drivers that let go of it when auto ends.
        if (haveValue) src.setExposureAbsolute(current);
        state.exposureLocked = true;
        return true;
    }

    bool unlockExposure(FrameSource& src, uint32_t fps, ControlState& state, std::string& error)
    {
        ControlReport r;
        setExposureForPreset(src, state.preset, fps, r);
        if (!r.unsupported.empty() && r.applied.empty())
        {
            error = "source refused the exposure controls";
            return false;
        }
        state.exposureLocked = false;
        return true;
    }

    bool lockWhiteBalance(FrameSource& src, ControlState& state, std::string& error)
    {
        if (!src.setAutoWhiteBalance(false))
        {
            error = "source cannot switch white balance to manual";
            return false;
        }
        state.wbLocked = true;
        return true;
    }

    bool unlockWhiteBalance(FrameSource& src, ControlState& state, std::string& error)
    {
        if (!src.setAutoWhiteBalance(true))
        {
            error = "source cannot switch white balance to auto";
            return false;
        }
        state.wbLocked = false;
        return true;
    }
}
