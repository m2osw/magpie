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
 * \brief Implementation of the recursive_rwlock class.
 *
 * This file implements the recursive_rwlock.
 *
 * \note
 * There are two drawbacks to this lock: to allow for the recursivity,
 * it makes use of a mutex. That's a contention point (even if really
 * short, it is necessary when locking and unlocking the rwlock).
 * \par
 * The second issue is that it gives readers priority meaning that
 * a set() may block and be starved for a while until all the
 * reader threads are done with the cache. In my experience with
 * such caches, this is rarely an issue since all the readers are
 * done quickly. If you do not want to deal with such contentions
 * then a mutex lock is what you need to use.
 */

// self
//
#include    <magpie/lock_recursive_rwlock.h>



// last include
//
#include    <snapdev/poison.h>



namespace magpie
{
namespace detail
{



lock_recursive_rwlock::~lock_recursive_rwlock()
{
}


char const * lock_recursive_rwlock::get_name() const
{
    return "rwlock";
}


bool lock_recursive_rwlock::supports_recursivity() const
{
    return true;
}


bool lock_recursive_rwlock::supports_rlock() const
{
    return true;
}



void lock_recursive_rwlock::lock()
{
    recursive_rwlock::lock();
}


void lock_recursive_rwlock::rlock()
{
    recursive_rwlock::rlock();
}


bool lock_recursive_rwlock::timed_lock(timespec const & wait)
{
    return recursive_rwlock::timed_lock(wait);
}


bool lock_recursive_rwlock::timed_rlock(timespec const & wait)
{
    return recursive_rwlock::timed_rlock(wait);
}


void lock_recursive_rwlock::unlock()
{
    recursive_rwlock::unlock();
}




} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
