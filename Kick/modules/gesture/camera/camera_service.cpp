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

#include "camera_service.h"

#include "contracts/gesture/gesture_signals.h"
#include "../core/backoff.h"
#include "../core/camera_status.h"

#include <atomic>
#include <cstdlib>
#include <algorithm>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace kick_gesture
{
    namespace
    {
        constexpr int kPollMs = 1000;

        // Monotonic milliseconds without the 49 day wrap of Time::getMillisecondCounter, which would leave the backoff
        // waiting for a time that has already passed.
        uint64_t nowMs() { return static_cast<uint64_t>( juce::Time::getMillisecondCounterHiRes() ); }
    }

    struct CameraService::Impl : public juce::Thread
    {
        Impl( std::string path ) : juce::Thread( "kick-camera-poll" ), socketPath( std::move( path ) ) {}
        ~Impl() override { stopThread( 2000 ); }

        void run() override
        {
            Backoff                 backoff( 1000, 30000 );
            contracts::CameraStatus last;
            bool                    haveLast = false;

            while( !threadShouldExit() )
            {
                // A pending enable or disable goes first, and always retries at once.
                int want = pending.exchange( kNone );
                if( want != kNone )
                {
                    std::string reply, error;
                    cameraRequest( socketPath, want == kOn ? "enable" : "disable", reply, error );
                    backoff.success();
                }

                const uint64_t now = nowMs();
                contracts::CameraStatus status = last;
                int                     waitMs = kPollMs;
                if( backoff.due( now ) )
                {
                    std::string reply, error;
                    if( cameraRequest( socketPath, "status", reply, error ) )
                    {
                        status = parseCameraStatus( reply );
                        // A reply that cannot be read counts as a failure. Otherwise the loop would ask again every 100 ms.
                        if( status.state == contracts::CameraState::ServiceMissing )
                            backoff.failure( now );
                        else
                            backoff.success();
                    }
                    else
                    {
                        backoff.failure( now );
                        status = cameraServiceMissing( error );
                    }
                }
                if( !backoff.due( now ) || status.state == contracts::CameraState::ServiceMissing )
                    waitMs = static_cast<int>( std::max<uint64_t>( 100, std::min<uint64_t>( backoff.remainingMs( nowMs() ), 30000 ) ) );

                // Only a change goes to the UI. The detail text of a missing service changes with the errno, so compare state alone for it.
                const bool changed = !haveLast || last.state != status.state || last.capturing != status.capturing
                                     || ( status.state == contracts::CameraState::Error && last.detail != status.detail );
                if( changed )
                {
                    last     = status;
                    haveLast = true;
                    juce::MessageManager::callAsync( [status] { contracts::GestureSignals::cameraStatusChanged( status ); } );
                }
                wake.wait( waitMs );
            }
        }

        static constexpr int kNone = 0, kOn = 1, kOff = 2;

        std::string        socketPath;
        std::atomic<int>   pending{ kNone };
        juce::WaitableEvent wake{ false };
    };

    std::string CameraService::defaultSocketPath()
    {
        if( const char* env = std::getenv( "KICK_CAMERA_SOCKET" ) )
            if( *env )
                return env;
        return "/run/kick-camera.sock";
    }

    CameraService::CameraService( std::string socketPath ) : impl( new Impl( socketPath.empty() ? defaultSocketPath() : std::move( socketPath ) ) ) {}

    CameraService::~CameraService() { stop(); }

    void CameraService::start()
    {
        if( !impl->isThreadRunning() )
            impl->startThread();
    }

    void CameraService::stop()
    {
        if( impl->isThreadRunning() )
        {
            impl->signalThreadShouldExit();
            impl->wake.signal();
            impl->stopThread( 2000 );
        }
    }

    void CameraService::requestEnabled( bool on )
    {
        impl->pending = on ? Impl::kOn : Impl::kOff;
        impl->wake.signal();
    }

    CameraService& sharedCameraService()
    {
        static CameraService service;
        return service;
    }
}
