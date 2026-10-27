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

#ifndef KICK_LOOK_AND_FEEL_H
    #define KICK_LOOK_AND_FEEL_H

namespace ui
{

    class KickLooknFeel : public juce::LookAndFeel_V4, public juce::DeletedAtShutdown
    {
    public:
        KickLooknFeel();
        static KickLooknFeel* getLnf()
        {
            // Its Drawable components must die before Desktop/message services.
            juce::Desktop::getInstance();
            static auto* instance = new KickLooknFeel;
            return instance;
        };

    #if defined( DEBUG_TOUCH ) && ( DEBUG_TOUCH > 0 )
        juce::MouseCursor getMouseCursorFor( juce::Component& component )
        {
            return juce::MouseCursor( juce::MouseCursor::DraggingHandCursor );
        }
    #endif

    private:
        void                                drawLabel( juce::Graphics& g, juce::Label& label ) override;
        void                                drawButtonBackground( juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown ) override;
        void                                drawButtonText( juce::Graphics& g, juce::TextButton& button, bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/ ) override;
        void                                drawTabButton( juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown ) override;
        void                                drawLinearSlider( juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider ) override;
        void                                drawRotarySlider( juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& ) override;
        void                                drawToggleButton( juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown ) override;
        void                                drawScrollbar( juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                                                           bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, [[maybe_unused]] bool isMouseDown ) override;
        void                                fillTextEditorBackground( juce::Graphics&, int width, int height, juce::TextEditor& ) override;
        std::unique_ptr<juce::FocusOutline> createFocusOutlineForComponent( juce::Component& ) override;
        void                                drawLabelText( juce::Graphics&, juce::Label& label, mt::MTProperties& props, juce::Rectangle<int>& bounds );
        void                                drawButtonText( juce::Graphics&, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown, mt::MTProperties& props, juce::Rectangle<int>& bounds );

    private:
        std::unique_ptr<juce::Drawable> rotary_background;
        std::unique_ptr<juce::Drawable> rotary_knob;
        std::unique_ptr<juce::Drawable> button_on;
        std::unique_ptr<juce::Drawable> button_off;
        std::unique_ptr<juce::Drawable> dropdown;

        std::unique_ptr<juce::Drawable> loadImage;
    };

} // Namespace :: ui
#endif // KICK_LOOK_AND_FEEL_H
