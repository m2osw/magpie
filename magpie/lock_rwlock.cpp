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
 * \brief The cache setup is done using these options.
 *
 * The mapgpie::cache class must be initialized in the constructor
 * because once initialized, changes to the settings are not permitted.
 * For that reason, it uses an options class to pre-define all the
 * settings.
 */

// self
//
#include    <magpie/lock_rwlock.h>



// snapdev
//
//#include    <snapdev/not_used.h>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{
namespace detail
{



lock_rwlock::~lock_rwlock()
{
}


char const * lock_rwlock::get_name() const
{
    return "rwlock";
}


bool lock_rwlock::supports_recursivity() const
{
    return false;
}


bool lock_rwlock::supports_rlock() const
{
    return true;
}



void lock_rwlock::lock()
{
    rwlock::lock();
}


void lock_rwlock::rlock()
{
    rwlock::rlock();
}


bool lock_rwlock::timed_lock(timespec const & wait)
{
    return rwlock::timed_lock(wait);
}


bool lock_rwlock::timed_rlock(timespec const & wait)
{
    return rwlock::timed_rlock(wait);
}


void lock_rwlock::unlock()
{
    rwlock::unlock();
}




} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
