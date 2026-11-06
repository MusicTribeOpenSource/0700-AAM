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
#include "kick_camera/shm_region.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace kickcam
{
    ShmRegion::~ShmRegion() { close(); }

    void ShmRegion::close()
    {
        if (data_ != nullptr) munmap(data_, size_);
        if (owner_ && !name_.empty()) shm_unlink(name_.c_str());
        data_ = nullptr;
        size_ = 0;
        owner_ = false;
        name_.clear();
    }

    bool ShmRegion::create(const std::string& name, size_t bytes, std::string& error)
    {
        close();
        shm_unlink(name.c_str());
        const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
        if (fd < 0) { error = "shm_open " + name + ": " + std::strerror(errno); return false; }
        if (ftruncate(fd, static_cast<off_t>(bytes)) != 0)
        {
            error = "ftruncate " + name + ": " + std::strerror(errno);
            ::close(fd);
            shm_unlink(name.c_str());
            return false;
        }
        void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);
        if (p == MAP_FAILED)
        {
            error = "mmap " + name + ": " + std::strerror(errno);
            shm_unlink(name.c_str());
            return false;
        }
        name_ = name;
        data_ = p;
        size_ = bytes;
        owner_ = true;
        return true;
    }

    bool ShmRegion::openReadOnly(const std::string& name, std::string& error)
    {
        close();
        const int fd = shm_open(name.c_str(), O_RDONLY, 0);
        if (fd < 0) { error = "shm_open " + name + ": " + std::strerror(errno); return false; }
        struct stat st;
        if (fstat(fd, &st) != 0 || st.st_size <= 0)
        {
            error = "fstat " + name + ": " + std::strerror(errno);
            ::close(fd);
            return false;
        }
        void* p = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_SHARED, fd, 0);
        ::close(fd);
        if (p == MAP_FAILED) { error = "mmap " + name + ": " + std::strerror(errno); return false; }
        name_ = name;
        data_ = p;
        size_ = static_cast<size_t>(st.st_size);
        owner_ = false;
        return true;
    }
}
