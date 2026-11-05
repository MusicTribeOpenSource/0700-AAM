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

#include "../../../common/source/kick_paths.h"
#include <cmath>

namespace mt
{
    /// <summary>
    ///  ASoundCache
    /// </summary>
    ///
    ASoundCache& getAudioSoundCache()
    {
        static ASoundCache cache;
        return cache;
    }

    ASoundCache::ASoundCache()
    {
        fmtMgr.registerBasicFormats();
    }

    ASoundCache::~ASoundCache()
    {
        clear();
    }

    void ASoundCache::clear()
    {
        aSounds.clear();
    }

    void ASoundCache::purge()
    {
        for( const auto& it : aSounds )
        {
            return it->purge();
        }
    }

    ASound::Ptr ASoundCache::exists( const juce::String& File )
    {
        for( auto it : aSounds )
        {
            if( 0 == File.compare( it->getFilePath() ) )
                return it;
        }
        return nullptr;
    }

    ASound::Ptr ASoundCache::getOrLoad( const juce::String& File, double LengthInSeconds )
    {
        if( auto ptr = exists( File ) )
            return ptr;

        return add( File );
    }

    void ASoundCache::remove( const juce::String& File )
    {
        aSounds.removeObject( exists( File ) );
    }

#define ASOUND_UPSAMPLE_RATIO ( 1 )

    ASound::Ptr ASoundCache::add( const juce::String& File, double LengthInSeconds )
    {
        if( File.isNotEmpty() )
        {
            if( juce::File( File ).existsAsFile() )
            {
                if( auto ptr = new ASound( File, fmtMgr, ASOUND_UPSAMPLE_RATIO, LengthInSeconds ) )
                {
                    return aSounds.add( ptr );
                }
            }
        }
        return nullptr;
    }

    static bool generateFallbackMetronomeWav( const juce::File& file, int styleIndex )
    {
        file.getParentDirectory().createDirectory();

        constexpr double sampleRate = 44100.0;
        constexpr double durationSeconds = 0.04; // 40 ms
        const int numSamples = static_cast<int>( sampleRate * durationSeconds );

        juce::AudioBuffer<float> buffer( 2, numSamples );
        buffer.clear();

        double baseFreq = 1600.0;
        double decayFactor = 120.0;

        switch( styleIndex )
        {
            case 1: baseFreq = 2400.0; decayFactor = 140.0; break; // Glass
            case 2: baseFreq = 1200.0; decayFactor = 90.0;  break; // Clap
            case 3: baseFreq = 3000.0; decayFactor = 160.0; break; // Metal
            case 4: baseFreq = 850.0;  decayFactor = 80.0;  break; // Wood pop
            case 5: baseFreq = 2000.0; decayFactor = 150.0; break; // Quartz
            case 6: baseFreq = 1400.0; decayFactor = 120.0; break; // Stick
            case 7: baseFreq = 1760.0; decayFactor = 70.0;  break; // Bell
            default: baseFreq = 1600.0; decayFactor = 120.0; break; // Standard
        }

        for( int i = 0; i < numSamples; ++i )
        {
            double t = static_cast<double>( i ) / sampleRate;
            double env = std::exp( -decayFactor * t );
            double sample = env * ( 0.85 * std::sin( 2.0 * juce::MathConstants<double>::pi * baseFreq * t )
                                  + 0.15 * std::sin( 4.0 * juce::MathConstants<double>::pi * baseFreq * t ) );
            buffer.setSample( 0, i, static_cast<float>( sample ) );
            buffer.setSample( 1, i, static_cast<float>( sample ) );
        }

        juce::WavAudioFormat wavFormat;
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> outputStream( file.createOutputStream() );
        if( !outputStream ) return false;

        std::unique_ptr<juce::AudioFormatWriter> writer( wavFormat.createWriterFor( outputStream.get(), sampleRate, 2, 16, {}, 0 ) );
        if( writer )
        {
            outputStream.release();
            writer->writeFromAudioSampleBuffer( buffer, 0, numSamples );
            return true;
        }
        return false;
    }

    ASound::Ptr ASoundCache::getOrLoadMetronomeSound( const juce::String& File )
    {
        for( auto it : metronomeSounds )
        {
            if( 0 == File.compare( it->getFilePath() ) )
                return it;
        }

        if( File.isNotEmpty() )
        {
            juce::File targetFile( File );
            if( !targetFile.existsAsFile() )
            {
                int styleIdx = 0;
                auto fn = targetFile.getFileName();
                if( fn.containsIgnoreCase( "Glass" ) ) styleIdx = 1;
                else if( fn.containsIgnoreCase( "Clap" ) ) styleIdx = 2;
                else if( fn.containsIgnoreCase( "Metal" ) ) styleIdx = 3;
                else if( fn.containsIgnoreCase( "MouthPop" ) ) styleIdx = 4;
                else if( fn.containsIgnoreCase( "Quartz" ) ) styleIdx = 5;
                else if( fn.containsIgnoreCase( "Stick" ) ) styleIdx = 6;
                else if( fn.containsIgnoreCase( "Bell" ) ) styleIdx = 7;

                if( !generateFallbackMetronomeWav( targetFile, styleIdx ) )
                {
                    auto fallback = getAppDirs( MTTemp ).getChildFile( "metronome" ).getChildFile( fn );
                    fallback.getParentDirectory().createDirectory();
                    if( generateFallbackMetronomeWav( fallback, styleIdx ) )
                        targetFile = fallback;
                }
            }

            if( targetFile.existsAsFile() )
            {
                if( auto ptr = new ASound( targetFile.getFullPathName(), fmtMgr, ASOUND_UPSAMPLE_RATIO ) )
                {
                    return metronomeSounds.add( ptr );
                }
            }
        }
        return nullptr;
    }

#undef ASOUND_UPSAMPLE_RATIO

} // namespace mt
