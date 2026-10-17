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
#include    "lock_interface.h"



// C++
//
#include    <cstdint>


// snapdev
//
//#include    <snapdev/not_used.h>



namespace magpie
{



class lock_guard
{
public:
                                lock_guard(lock_interface::pointer_t lock, bool exclusive);
                                lock_guard(lock_interface::pointer_t lock, timespec const & wait, bool exclusive);
                                lock_guard(lock_guard const &) = delete;
                                ~lock_guard();

    lock_guard &                operator = (lock_guard const &) = delete;

    void                        unlock();
    bool                        is_locked() const;

private:
    lock_interface::pointer_t   f_lock = lock_interface::pointer_t();
    bool                        f_locked = false;
};



} // namespace magpie
// vim: ts=4 sw=4 et
