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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace kicktest
{
    // A scratch directory removed (non-recursively into subdirectories, which these tests never make) on destruction.
    // It lives under /tmp, not $TMPDIR, because Unix socket paths are limited to about 100 bytes.
    class TempDir
    {
    public:
        TempDir()
        {
            char tmpl[] = "/tmp/kcam-XXXXXX";
            const char* p = ::mkdtemp(tmpl);
            path_ = p != nullptr ? p : "";
        }
        ~TempDir()
        {
            for (const std::string& f : list()) ::unlink((path_ + "/" + f).c_str());
            ::rmdir(path_.c_str());
        }
        const std::string& path() const { return path_; }
        std::string file(const std::string& name) const { return path_ + "/" + name; }
        std::vector<std::string> list() const
        {
            std::vector<std::string> names;
            if (DIR* d = ::opendir(path_.c_str()))
            {
                while (const dirent* e = ::readdir(d))
                {
                    const std::string n = e->d_name;
                    if (n != "." && n != "..") names.push_back(n);
                }
                ::closedir(d);
            }
            return names;
        }

    private:
        std::string path_;
    };

    inline void writeFile(const std::string& path, const std::vector<uint8_t>& bytes)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    inline void writeFile(const std::string& path, const std::string& text)
    {
        writeFile(path, std::vector<uint8_t>(text.begin(), text.end()));
    }

    // A short unique shm name (macOS limits names to 31 characters).
    inline std::string uniqueShmName(const char* tag)
    {
        static int counter = 0;
        return std::string("/kc-") + tag + "-" + std::to_string(::getpid() % 100000) + "-" + std::to_string(counter++);
    }
}
