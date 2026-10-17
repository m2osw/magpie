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
 * \brief The interface used to lock/unlock.
 *
 * The cache works in a multi-threaded environment. Some of the features
 * require the work to be done sequentially, such as reading or writing
 * the cache data.
 *
 * To allow for varying types of capabilities, this interfaces offers
 * the magpie implementation to choose a lock or another. At the moment
 * the following are supported:
 *
 * \li mutex -- in this mode, all the accesses to the cache data is locked
 * by one thread; all the other threads are blocked until this one is done
 * whatever the work being done is;
 * \li rwlock -- in this mode, the lock offers a read-only version allowing
 * any number of threads to read from the cache in parallel; some of the
 * features, however, are not available (a.k.a. the atomic inc() and
 * dec() functions)
 * \li recursive_rwlock -- this is similar to the rwlock, except that it
 * allows recursivity which means the same thread can lock the lock any
 * number of times (i.e. this means atomic functions are available); the
 * drawback of this lock is that it uses a mutex and any lock or unlock
 * action locks that mutex twice on top of the rwlock
 *
 * The mutex is ideal if you want the cache to be fast and the number
 * of reads and writes is rather limited and more likely about equal.
 *
 * The rwlock is the fastest, but as mentioned above, prevents certain
 * features from working because it is not re-entrant.
 *
 * The recursive_rwlock is the slowest, but is still giving a visible
 * improvement for caches that are heavily used to read the cached values.
 */

// self
//
#include    <magpie/lock_interface.h>

//#include    <magpie/exception.h>



// snapdev
//
//#include    <snapdev/not_used.h>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{



/** \brief Make sure the destructor exists.
 *
 * The interface requires a valid destructor which this is.
 */
lock_interface::~lock_interface()
{
}



} // namespace magpie
// vim: ts=4 sw=4 et
