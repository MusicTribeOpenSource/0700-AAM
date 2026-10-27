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

#include <stack>

namespace kick_ui
{
    class KickLEDs;
    class AppStatusbar;
    class KickHome : public juce::Component
        , private juce::AsyncUpdater
        , private mt::MTAudioInterfaceListener
    {
    public:
        static KickHome& Instance()
        {
            static KickHome _instance;
            return _instance;
        }

        inline Screen getCurrentScreen() const { return currentScreen; }
        inline Screen getPreviousScreen() const { return stack.empty() ? Screen::HomeListView : stack.top(); }
        IScreen*      getScreen( Screen screen );

        void AssignSampleStartUp( void );

    private:
        KickHome();
        ~KickHome();

        void                 load( Screen screen );
        void                 handleCommand( CPktMgr::HWMsg_t& );
        void                 knobCommand( int knob, int value );
        void                 resized() override;
        void                 handleAsyncUpdate() override;
        void                 onProjectSaveFailed(const juce::String&) override;
        void                 onProjectDirtyChanged(bool) override;
        inline void          setupLoadScreenVisibility();
        juce::Rectangle<int> getScreenBounds( IScreen* pScreen );

    private:
        std::stack<Screen>                         stack;
        std::map<Screen, std::unique_ptr<IScreen>> screens_;

        std::unique_ptr<Component>                 ctrlBar_;
        std::unique_ptr<KickLEDs>                  ledControl_;
        juce::Component::SafePointer<AppStatusbar> statusBar_;
        int                                        page{ 0 };

        Screen currentScreen{ Screen::Invalid };
        Screen prevScreen{ Screen::Invalid };
    };

    static void padPress( const std::vector<int>& Notes, int Velocity, mt::SamplePlayType PlayType );
    static void padRelease( const std::vector<int>& Notes, int Velocity, mt::SamplePlayType PlayType );
}
