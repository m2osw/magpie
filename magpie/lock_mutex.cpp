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
#include    <magpie/lock_mutex.h>

//#include    <magpie/exception.h>



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



lock_mutex::~lock_mutex()
{
}


char const * lock_mutex::get_name() const
{
    return "mutex";
}


bool lock_mutex::supports_recursivity() const
{
    return true;
}


bool lock_mutex::supports_rlock() const
{
    return false;
}



void lock_mutex::lock()
{
    mutex::lock();
}


void lock_mutex::rlock()
{
    mutex::lock();
}


bool lock_mutex::timed_lock(timespec const & wait)
{
    return mutex::timed_wait(wait);
}


bool lock_mutex::timed_rlock(timespec const & wait)
{
    return mutex::timed_wait(wait);
}


void lock_mutex::unlock()
{
    mutex::unlock();
}



} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
