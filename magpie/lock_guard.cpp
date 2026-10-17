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


/** \file
 * \brief Lock one of our locks.
 *
 * The cache can make use of different types of locks. This guard is
 * compatible with the cache lock interface.
 */

// self
//
#include    <magpie/lock_guard.h>

#include    <magpie/exception.h>



// snapdev
//
//#include    <snapdev/not_used.h>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{



lock_guard::lock_guard(lock_interface::pointer_t lock, bool exclusive)
    : f_lock(lock)
    , f_locked(true)
{
    if(exclusive)
    {
        f_lock->lock();
    }
    else
    {
        f_lock->rlock();
    }
}


lock_guard::lock_guard(lock_interface::pointer_t lock, timespec const & wait, bool exclusive)
    : f_lock(lock)
{
    if(exclusive)
    {
        f_locked = f_lock->timed_lock(wait);
    }
    else
    {
        f_locked = f_lock->timed_rlock(wait);
    }
}


lock_guard::~lock_guard()
{
    unlock();
}


void lock_guard::unlock()
{
    if(f_locked)
    {
        f_lock->unlock();
        f_locked = false;
    }
}


bool lock_guard::is_locked() const
{
    return f_locked;
}



} // namespace magpie
// vim: ts=4 sw=4 et
