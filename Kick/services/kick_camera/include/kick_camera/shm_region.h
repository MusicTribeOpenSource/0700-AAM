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

namespace kickcam
{
    // A named POSIX shared-memory object mapped into this process.
    // The creating side owns the name and unlinks it when the region is destroyed.
    class ShmRegion
    {
    public:
        ShmRegion() = default;
        ~ShmRegion();
        ShmRegion(const ShmRegion&) = delete;
        ShmRegion& operator=(const ShmRegion&) = delete;

        // Replaces any object of the same name. Mode 0600: only the owner can map it, since the frames are camera images. Consumers run as the same user.
        bool create(const std::string& name, size_t bytes, std::string& error);

        // Maps an existing object read-only.
        bool openReadOnly(const std::string& name, std::string& error);

        void close();

        void* data() const { return data_; }
        size_t size() const { return size_; }

    private:
        std::string name_;
        void* data_ = nullptr;
        size_t size_ = 0;
        bool owner_ = false;
    };
}
