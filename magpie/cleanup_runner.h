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
#include    <magpie/cache_impl.h>



// cppthread
//
#include    <cppthread/fifo.h>
#include    <cppthread/runner.h>


// snapdev
//
#include    <snapdev/timespec_ex.h>


// C++
//
#include    <list>


// C
//
//#include    <time.h>



namespace magpie
{
namespace detail
{



class cleanup_runner
    : public cppthread::runner
{
public:
    typedef std::shared_ptr<cleanup_runner>     pointer_t;

                                cleanup_runner();
    virtual                     ~cleanup_runner() override;

    virtual void                run() override;

    void                        manage_cache(cache_impl::pointer_t impl);
    void                        release_cache(cache_impl::pointer_t impl);
    void                        wakeup();
    bool                        all_caches_removed() const;
    //bool                        cleanup_removed_caches();

private:
    struct tracker_t
    {
        typedef std::shared_ptr<tracker_t>  pointer_t;
        typedef std::list<pointer_t>        list_t;

                                    tracker_t(
                                          cache_impl::weak_pointer_t cache
                                        , snapdev::timespec_ex next_cleanup);

        void                        compute_next_cleanup();

        cache_impl::weak_pointer_t  f_cache = cache_impl::weak_pointer_t();

        // current state
        //
        snapdev::timespec_ex        f_next_cleanup = snapdev::timespec_ex();
        bool                        f_active = false;
        bool                        f_purge_expired = false;
        bool                        f_evict_stats = false;
        bool                        f_evict_organize_by_bucket = false;
        bool                        f_evict = false;
    };

    struct request_t
    {
        cache_impl::weak_pointer_t  f_cache = cache_impl::weak_pointer_t();
        hash_t                      f_hash = hash_t();
    };

    typedef cppthread::fifo<request_t>      fifo_t;

    snapdev::timespec_ex        next_wakeup_call() const;
    void                        handle_request(request_t const & r);
    void                        cleanup_caches();
    bool                        purge_expired(tracker_t &t);
    bool                        evict_stats_needed(tracker_t &t);
    bool                        evict_stats(tracker_t &t);
    bool                        evict_organize_by_bucket(tracker_t &t);
    bool                        evict(tracker_t & t);

    tracker_t::list_t           f_caches = tracker_t::list_t();
    tracker_t::list_t::iterator
                                f_caches_cleanup_it = tracker_t::list_t::iterator();
    fifo_t                      f_fifo = fifo_t();
};



} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
