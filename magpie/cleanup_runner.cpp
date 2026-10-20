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
#include    <magpie/cleanup_runner.h>

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



cleanup_runner::tracker_t::tracker_t(
          cache_impl::weak_pointer_t cache
        , snapdev::timespec_ex next_cleanup)
    : f_cache(cache)
    , f_next_cleanup(next_cleanup)
{
std::cerr << "--- cleanup runner was created...\n";
}


void cleanup_runner::tracker_t::compute_next_cleanup()
{
    cache_impl::pointer_t cache(f_cache.lock());
    if(cache != nullptr)
    {
        duration_t const interval(cache->get_cleanup_interval());
        duration_t const diff(snapdev::now() - f_next_cleanup);
        int const intervals_to_add(diff / interval + 1);
        f_next_cleanup += interval * intervals_to_add;
    }
}







cleanup_runner::cleanup_runner()
    : runner("cache_cleanup_worker")
    , f_caches_cleanup_it(f_caches.end())
{
}


cleanup_runner::~cleanup_runner()
{
}


void cleanup_runner::run()
{
    snapdev::timespec_ex const min_sleep(snapdev::timespec_ex(0.25)); // 250ms
    while(continue_running())
    {
        snapdev::timespec_ex const wakeup_at(next_wakeup_call());
        snapdev::timespec_ex now(snapdev::now());

        snapdev::timespec_ex const sleep_for(std::max(wakeup_at - now, min_sleep));
        request_t r;
        if(f_fifo.pop_front(r, sleep_for.to_usec()))
        {
            // received a request, handle it
            //
            handle_request(r);
        }

        now = snapdev::now();
        if(!wakeup_at
        || wakeup_at <= now)
        {
std::cerr << "--- call cleanup_caches()!\n";
            cleanup_caches();
std::cerr << "--- cleanup_caches() returned!\n";
        }
else std::cerr << "--- skip cleanup_caches() call?!\n";

        if(all_caches_removed())
        {
            // no more caches to manage
            //
            break;
        }

        //if(cleanup_removed_caches())
        //{
        //    // no more caches to manage
        //    //
        //    break;
        //}
    }
}


void cleanup_runner::manage_cache(cache_impl::pointer_t impl)
{
    cppthread::guard lock(f_mutex);

    // cache already in our list?
    //
    auto it(std::find_if(
              f_caches.begin()
            , f_caches.end()
            , [&impl](tracker_t::pointer_t t)
            {
                return impl == t->f_cache.lock();
            }));
    if(it != f_caches.end())
    {
        return;
    }

    // insert new cache
    //
    f_caches.emplace_back(std::make_shared<tracker_t>(impl, snapdev::now() + impl->get_cleanup_interval()));

    wakeup();
}


void cleanup_runner::release_cache(cache_impl::pointer_t impl)
{
    cppthread::guard lock(f_mutex);

    auto it(std::find_if(
              f_caches.begin()
            , f_caches.end()
            , [&impl](tracker_t::pointer_t t)
            {
                return impl == t->f_cache.lock();
            }));
    if(it != f_caches.end())
    {
        if(it == f_caches_cleanup_it)
        {
            f_caches_cleanup_it = f_caches.erase(it);
        }
        else
        {
            f_caches.erase(it);
        }
    }
}


void cleanup_runner::wakeup()
{
    // send an empty request to wake-up the runner thread
    //
    // this is useful when adding or removing a cache
    //
    request_t r;
    f_fifo.push_back(r);
}


bool cleanup_runner::all_caches_removed() const
{
    cppthread::guard lock(f_mutex);
    return f_caches.empty();
}


// at the moment, all caches will always be removed explicitly so we
// should never have to check whether it was gone under our feet
//bool cleanup_runner::cleanup_removed_caches()
//{
//    cppthread::guard lock(f_mutex);
//    auto it(f_caches.begin());
//    while(it != f_caches.end())
//    {
//        if((*it)->f_cache.lock() == nullptr)
//        {
//            it = f_caches.erase(it);
//        }
//        else
//        {
//            ++it;
//        }
//    }
//
//    return f_caches.empty();
//}


snapdev::timespec_ex cleanup_runner::next_wakeup_call() const
{
    snapdev::timespec_ex smallest(snapdev::timespec_ex::timespec_ex::max());

    cppthread::guard lock(f_mutex);
    for(auto it : f_caches)
    {
        if(it->f_active)
        {
            return snapdev::timespec_ex();
        }
        smallest = std::min(smallest, it->f_next_cleanup);
    }

    return smallest;
}


void cleanup_runner::handle_request(request_t const & r)
{
    cache_impl::pointer_t cache(r.f_cache.lock());
    if(cache == nullptr)
    {
        return;
    }

    cache->erase(r.f_hash);
}


void cleanup_runner::cleanup_caches()
{
    cppthread::guard lock(f_mutex);

    if(f_caches_cleanup_it == f_caches.end())
    {
        f_caches_cleanup_it = f_caches.begin();
    }
    if(f_caches_cleanup_it != f_caches.end())
    {
        if(!(*f_caches_cleanup_it)->f_active)
        {
            if((*f_caches_cleanup_it)->f_next_cleanup <= snapdev::now())
            {
                (*f_caches_cleanup_it)->f_active = true;
                (*f_caches_cleanup_it)->f_purge_expired = true;
                (*f_caches_cleanup_it)->f_evict_stats = evict_stats_needed(**f_caches_cleanup_it);
                (*f_caches_cleanup_it)->f_evict = false;
            }
        }
        if((*f_caches_cleanup_it)->f_active)
        {
            if((*f_caches_cleanup_it)->f_purge_expired)
            {
std::cerr << "--- cleanup_caches() -> purge expired check\n";
                if(purge_expired(**f_caches_cleanup_it))
                {
std::cerr << "--- cleanup_caches() -> purge expired done\n";
                    (*f_caches_cleanup_it)->f_purge_expired = false;
                }
else std::cerr << "--- cleanup_caches() -> purge expired returned\n";
            }

            if((*f_caches_cleanup_it)->f_evict_stats)
            {
std::cerr << "--- cleanup_caches() -> evict stats check\n";
                if(evict_stats(**f_caches_cleanup_it))
                {
std::cerr << "--- cleanup_caches() -> evict stats done\n";
                    (*f_caches_cleanup_it)->f_evict_stats = false;
                    (*f_caches_cleanup_it)->f_evict_organize_by_bucket = true;
                }
else std::cerr << "--- cleanup_caches() -> evict stats returned\n";
            }

            if((*f_caches_cleanup_it)->f_evict_organize_by_bucket)
            {
std::cerr << "--- cleanup_caches() -> evict organizing...\n";
                if(evict_organize_by_bucket(**f_caches_cleanup_it))
                {
std::cerr << "--- cleanup_caches() -> evict organizing done...\n";
                    (*f_caches_cleanup_it)->f_evict_organize_by_bucket = false;
                    (*f_caches_cleanup_it)->f_evict = true;
                }
else std::cerr << "--- cleanup_caches() -> evict organizing returned...\n";
            }

            if((*f_caches_cleanup_it)->f_evict)
            {
std::cerr << "--- cleanup_caches() -> evict now!\n";
                if(evict(**f_caches_cleanup_it))
                {
std::cerr << "--- cleanup_caches() -> evict now done\n";
                    (*f_caches_cleanup_it)->f_evict = false;
                }
else std::cerr << "--- cleanup_caches() -> evict now return\n";
            }

            if(!(*f_caches_cleanup_it)->f_purge_expired
            && !(*f_caches_cleanup_it)->f_evict_stats
            && !(*f_caches_cleanup_it)->f_evict_organize_by_bucket
            && !(*f_caches_cleanup_it)->f_evict)
            {
std::cerr << "--- cleanup_caches() -> loop done, reset state.\n";
                (*f_caches_cleanup_it)->f_active = false;
                (*f_caches_cleanup_it)->compute_next_cleanup();
            }
else std::cerr << "--- cleanup_caches() -> returning, state is still active...\n";
        }
    }
else std::cerr << "--- cleanup_caches() -> no caches to clean up...\n";
}


bool cleanup_runner::purge_expired(tracker_t & t)
{
    cache_impl::pointer_t cache(t.f_cache.lock());
    if(cache == nullptr)
    {
        return true;
    }

    return cache->purge_expired();
}


bool cleanup_runner::evict_stats_needed(tracker_t & t)
{
    cache_impl::pointer_t cache(t.f_cache.lock());
    if(cache == nullptr)
    {
        return true;
    }

    return cache->is_limit_count_reached()
        || cache->is_capacity_reached();
}


bool cleanup_runner::evict_stats(tracker_t & t)
{
    cache_impl::pointer_t cache(t.f_cache.lock());
    if(cache == nullptr)
    {
        return true;
    }

    return cache->evict_stats();
}


bool cleanup_runner::evict_organize_by_bucket(tracker_t &t)
{
    cache_impl::pointer_t cache(t.f_cache.lock());
    if(cache == nullptr)
    {
        return true;
    }

    return cache->evict_organize_by_bucket();
}


bool cleanup_runner::evict(tracker_t & t)
{
    cache_impl::pointer_t cache(t.f_cache.lock());
    if(cache == nullptr)
    {
        return true;
    }

    return cache->evict();
}



} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
