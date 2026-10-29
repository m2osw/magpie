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
 * \brief Interface implementing one of the locks supported by magpie.
 *
 * The cache can be used in a fully threaded environment. Because of
 * that the cache much often be locked when used. This implements an
 * interface allow us to support different types of locks.
 *
 * By default, the system uses a mutex. This is not the most ideal,
 * but it does work quickly and properly. However, it prevents
 * multiple threads from reading the same value. Each access is
 * mutually exclusive.
 *
 * The system currently supports two other types of locks: the
 * rwlock and the recursive_rwlock. The main problem with the
 * rwlock is that it is not recursive. At the moment, the only
 * functions that require recursivity are the add() functions
 * allowing you to atomically increment or decrement a value
 * in your cache.
 *
 * The rwlock is the best bet for most cases. It has one drawback,
 * though, it can starve the write calls.
 */

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
