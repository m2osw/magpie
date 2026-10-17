// Copyright (c) 2026  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/magpie
// contact@m2osw.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

/** \file
 * \brief The cache setup has to be done using these options.
 *
 * The cache must be initialized in the constructor because once
 * initialized, changes to the settings are not permitted.
 */

// self
//
//#include    "cache_impl.h"



// C++
//
#include    <cstdint>


// snapdev
//
//#include    <snapdev/not_used.h>



namespace magpie
{



enum class lock_model_t
{
    LOCK_MODEL_MUTEX,
    LOCK_MODEL_RWLOCK,
    LOCK_MODEL_RECURSIVE_RWLOCK,
};


class lock_manager
{
public:
                            lock_manager(lock_model_t model);

    void                    lock();
    void                    rlock();
    bool                    timed_lock(timespec const & wait);
    bool                    timed_rlock(timespec const & wait);
    void                    unlock();

private:
    lock_interface::pointer_t   f_lock = lock_interface::pointer_t();
};



} // namespace magpie
// vim: ts=4 sw=4 et
