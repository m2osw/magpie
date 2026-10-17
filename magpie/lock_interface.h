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



// snapdev
//
//#include    <snapdev/not_used.h>


// C++
//
#include    <memory>


// C
//
#include    <time.h>



namespace magpie
{



class lock_interface
{
public:
    typedef std::shared_ptr<lock_interface>     pointer_t;

    virtual                 ~lock_interface();

    virtual char const *    get_name() const = 0;
    virtual bool            supports_recursivity() const = 0;
    virtual bool            supports_rlock() const = 0;

    virtual void            lock() = 0;
    virtual void            rlock() = 0;
    virtual bool            timed_lock(timespec const & wait) = 0;
    virtual bool            timed_rlock(timespec const & wait) = 0;
    virtual void            unlock() = 0;
};



} // namespace magpie
// vim: ts=4 sw=4 et
