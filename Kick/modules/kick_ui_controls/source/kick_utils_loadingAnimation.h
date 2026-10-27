/*
 * Copyright (c) 2021 MUSIC Tribe
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

#include "kick_utils_blur.h"

namespace mt
{
    class fsPopup;

    namespace utils
    {
        class LoadAnimation
            : public juce::Component
            , protected juce::Timer
            , public juce::DeletedAtShutdown
        {
        public:
            std::function<void( bool )> onAnimationEnd;
            LoadAnimation();
            ~LoadAnimation();
            static LoadAnimation& getInstance()
            {
                static auto* instance = new LoadAnimation;
                return *instance;
            }

            inline bool isAnimationRunning() { return isAnimating.load(); };

            void startAnimation();
            void stopAnimation();
            void stopAnimationAndFree();

        private:
            void loadImages();
            void close();
            void timerCallback() override;
            void paint( juce::Graphics& ) override;

        private:
            struct Frames { std::vector<juce::Image> images; };
            juce::SharedResourcePointer<Frames> frames;
            std::atomic<bool>               isAnimating{ false };
            std::atomic<int>                id{ 0 };
            fsPopup*                        popup{ nullptr };
        };
    };

} // namespace ui
