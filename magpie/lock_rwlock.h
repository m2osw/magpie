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
 * \brief Declarations of the rwlock implementation.
 *
 * This file declares the rwlock.
 *
 * Note that this lock is created giving readers priority. This means
 * a set() may be starved while many other threads read values from
 * the cache.
 *
 * \warning
 * The rwlock is not recursive. That means you cannot double lock
 * the same lock (that would result in a deadlock).
 * \par
 * At the moment, the only functions that make use of the recursivity
 * are the add() and related functions. If you do not have any use
 * of those, then this lock is appropriate.
 */

// self
//
#include    <magpie/lock_interface.h>



// cppthread
//
#include    <cppthread/rwlock.h>



namespace magpie
{
namespace detail
{



class lock_rwlock
    : public lock_interface
    , private cppthread::rwlock
{
public:
    typedef std::shared_ptr<lock_rwlock>        pointer_t;

    virtual                 ~lock_rwlock() override;

    virtual char const *    get_name() const override;
    virtual bool            supports_recursivity() const override;
    virtual bool            supports_rlock() const override;

    virtual void            lock() override;
    virtual void            rlock() override;
    virtual bool            timed_lock(timespec const & wait) override;
    virtual bool            timed_rlock(timespec const & wait) override;
    virtual void            unlock() override;
};



} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
