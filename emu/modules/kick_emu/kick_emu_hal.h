/*
 * Hardware abstraction hooks for the KICK desktop emulator.
 *
 * Kick's hardware code talks to Linux paths (/dev/ttymxc0, /dev/i2c-3,
 * /sys/..., /mnt/sdcard) and shells out to system tools. In KICK_EMU builds
 * those touchpoints call into here instead. Everything lives under one
 * emulated device root (KICK_EMU_ROOT, default ~/KickEmu) laid out like the
 * real filesystem, so /mnt/sdcard becomes $KICK_EMU_ROOT/mnt/sdcard.
 *
 * This header has no JUCE dependency so it can be included from any Kick
 * source file.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace kick_emu
{
    /// Emulated device root, without a trailing slash.
    const char* root();

    /// Maps an absolute device path ("/mnt/sdcard") into the emulated root.
    /// The returned pointer stays valid for the life of the process.
    const char* devicePath( const char* linuxPath );

    /// True if the mapped path exists. Used for device-node checks such as
    /// /dev/mmcblk1 (SD card present) and /dev/sda (USB stick present).
    bool deviceExists( const char* linuxPath );

    /// True if $root/proc/mounts has a line containing Id ("sdcard", "flash").
    bool isMounted( const char* id );

    /// Replacement for system(). Logs the command and does not run it.
    /// Commands the emulator understands (reboot, poweroff) are forwarded to
    /// the Surface as events. Returns 0 like a successful shell command.
    int shell( const char* command );

    /// Virtual I2C bus. The CV DAC sits at address 0x0C on bus 3.
    bool i2cWrite( int bus, int address, const uint8_t* data, int length );
    bool i2cRead( int bus, int address, uint8_t* data, int length );

    /// Sends one line ("backlight 80") to the Surface over UDP.
    void notify( const char* line );
}
