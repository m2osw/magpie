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
 * \brief The main() function of the magpie library tests.
 *
 * This file implements the main() function of the unit tests used to
 * verify the magpie library.
 *
 * It defines any globals (none at this point) and a few basic command
 * line options such as --help and --version.
 */


// self
//
#include    <magpie/cache_interface.h>
#include    <magpie/lock_interface.h>
#include    <magpie/options.h>



// cppthread
//
//#include    <cppthread/mutex.h>


// snapdev
//
#include    <snapdev/math.h>
#include    <snapdev/preset_atomic.h>


// C++
//
#include    <map>
#include    <mutex>
#include    <set>
#include    <unordered_map>
#include    <vector>



namespace magpie
{



struct metadata_t
{
    bool is_defined() const
    {
        return operator bool ();
    }

    operator bool () const
    {
        return f_created_on != time_t();
    }

    std::uint32_t access_count() const
    {
        return static_cast<std::uint32_t>(f_update_count)
             + static_cast<std::uint32_t>(f_touch_count);
    }

    time_t last_accessed_on() const
    {
        return std::max(f_last_updated_on, f_last_touched_on);
    }

    std::size_t             f_size = 0;
    std::size_t             f_allocated = 0;
    std::uint16_t           f_update_count = 0;
    std::uint16_t           f_touch_count = 0;
    time_t                  f_created_on = time_t();
    time_t                  f_last_updated_on = time_t();
    time_t                  f_last_touched_on = time_t();
    time_t                  f_stale_at = time_t();
    time_t                  f_timeout = time_t();
    duration_t              f_ttl = NO_TTL;
    float                   f_cost = NO_COST;
    std::uint32_t           f_turn = 0;
};


/** \brief List of counters used for statistics purposes.
 *
 * This enumeration defines a setup of counters used to have
 * statistics of the number of set()/get() calls, the eviction
 * environment and other parts of the cache.
 */
enum class statistic_counter_t
{
    // set specific
    //
    SET_COUNTER_ADDED,
    SET_COUNTER_UPDATED,
    SET_COUNTER_REJECTED,           // in set(): TTL reached, item size too large, cache full
    SET_MAXIMUM_UPDATE_COUNT,
    SET_MAXIMUM_ALLOCATED,

    // get specific
    //
    GET_COUNTER_FOUND,
    GET_COUNTER_MISSING,
    GET_COUNTER_STALE,
    GET_COUNTER_AUTO_ERASE,         // in get(): TTL reached with STALE_DATA_DELETE
    GET_MAXIMUM_TOUCH_COUNT,

    // eviction specific
    //
    EVICTION_COUNTER_EXPIRED,
    EVICTION_COUNTER_ERASED,

    STATISTIC_COUNTER_max
};


struct statistics_t
{
    std::uint64_t           f_counter[static_cast<std::size_t>(statistic_counter_t::STATISTIC_COUNTER_max)] = {};
};


enum class set_result_t
{
    SET_RESULT_ADDED,
    SET_RESULT_UPDATED,
    SET_RESULT_REJECTED_CAPACITY,
    SET_RESULT_REJECTED_COUNT,
    SET_RESULT_REJECTED_TOO_LARGE,
    SET_RESULT_REJECTED_EXPIRED,
    SET_RESULT_REJECTED_BANNED,

    SET_RESULT_OTHER,
};



namespace detail
{



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
typedef unsigned __int128       hash_t;
#pragma GCC diagnostic pop


class cache_impl
    : public std::enable_shared_from_this<cache_impl>
{
private:
    struct item_t
    {
        typedef std::map<hash_t, item_t>        map_t;

                                item_t(cache_interface * parent);
                                item_t(item_t const & rhs);
                                ~item_t();

        void                    set_value(void const * value, std::size_t size);

        item_t const &          operator = (item_t const & rhs) = delete; // there is a commented out version of this -- at the moment it's not used so not tested, removing until really needed.

        // set once on creation -- first set()
        //
        cache_interface *       f_parent = nullptr;                         // to call the copy operator
        std::size_t             f_size = 0;                                 // size of the object
        biased_time_t           f_created_on = biased_time_t();             // the time when the object was created

        // updated on a set()
        //
        void *                  f_value = nullptr;                          // a copy of the user's object
        std::size_t             f_allocated = 0;                            // amount of memory used by this object
        float                   f_cost = NO_COST;                           // cost of evicting this item
        duration_t              f_ttl = duration_t();                       // ttl to update f_timeout
        biased_time_t           f_last_updated_on = biased_time_t();        // last time this very object was updated (last set)
        std::uint16_t           f_update_count = 1;                         // number of time the item was updated (set)

        // updated on a set() and/or get()
        //
        snapdev::preset_atomic<std::uint16_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_touch_count{0};                           // number of time the item was touched (get)
        snapdev::preset_atomic<biased_time_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_last_touched_on{biased_time_t()};         // last time this very object was read (last get only)
        snapdev::preset_atomic<biased_time_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_timeout{biased_time_t()};                 // when value becomes stale

        // updated by the eviction algorithm
        //
        std::uint32_t           f_turn = 0;                                 // number of time this item went through the eviction process
    };

public:
    typedef std::shared_ptr<cache_impl>     pointer_t;
    typedef std::weak_ptr<cache_impl>       weak_pointer_t;

                            cache_impl(
                                      cache_interface * parent
                                    , options const & opts);
                            cache_impl(cache_impl const &) = delete;

    cache_impl &            operator = (cache_impl const &) = delete;

    static hash_t           hash_key(void const * key, std::size_t size);
    void                    set_capacity_limit(std::size_t capacity);
    void                    set_count_limit(std::size_t count);
    duration_t              get_cleanup_interval() const;
    void                    cache_done();
    std::size_t             size() const;
    bool                    is_limit_count_reached(bool exact) const;
    std::size_t             allocated() const;
    bool                    is_capacity_reached(bool exact, std::size_t extra_bytes = 0) const;
    bool                    item_timed_out(item_t const & item) const;
    std::size_t             get_bucket_index(hash_t h) const;

    set_result_t            set(
                                  hash_t h
                                , void const * value
                                , std::size_t value_size
                                , duration_t ttl
                                , float cost);

    bool                    get(
                                  hash_t h
                                , void * value
                                , std::size_t value_size
                                , bool update_ttl);

    metadata_t              get_metadata(hash_t h) const;
    statistics_t            get_statistics(bool reset);

    bool                    erase(hash_t h);

    bool                    ban(hash_t h);
    void                    unban(hash_t h);

    bool                    contains(hash_t h) const;

    bool                    purge_expired();
    bool                    decay_counters();
    bool                    evict_stats();
    bool                    evict_weights();
    bool                    evict_organize_by_bucket();
    bool                    evict();

    lock_interface::pointer_t
                            get_lock(hash_t h);

#ifdef _DEBUG
    void                    verify_current_count();
    void                    verify_current_capacity();
#endif

    bool                    is_cleanup_worker_required() const;
    void                    start_cleanup_worker();
    void                    activate_eviction();

private:
    struct bucket_t
    {
        typedef std::vector<bucket_t>       vector_t;

                                bucket_t(lock_model_t lock_model);

        item_t::map_t           f_entries = item_t::map_t();
        lock_interface::pointer_t
                                f_lock = lock_interface::pointer_t();
    };

    struct evict_item_t
    {
        bool operator < (evict_item_t const & rhs) const
        {
            return f_weight < rhs.f_weight
                || (snapdev::quiet_floating_point_equal(f_weight, rhs.f_weight) && f_hash < rhs.f_hash);
        }

        hash_t                  f_hash = hash_t();
        double                  f_weight = 0.0;
        std::size_t             f_allocated = 0;
    };

    typedef std::set<evict_item_t>      smallest_evict_item_t;

    static constexpr std::size_t const  EVICT_SMALLEST_WEIGHT_COUNT = 100;

    typedef std::set<hash_t>            banned_key_t;
    typedef std::unordered_map<std::size_t, std::vector<hash_t>>
                                        organized_evict_items_t;

    void                        stats_inc(statistic_counter_t counter) { ++f_statistics[static_cast<int>(counter)]; }

    cache_interface *           f_parent = nullptr;
    options const               f_opts;
    banned_key_t                f_banned_keys = banned_key_t();
    bucket_t::vector_t          f_buckets = bucket_t::vector_t();
    snapdev::preset_atomic<std::size_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_current_capacity_usage{0};
    snapdev::preset_atomic<std::size_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_current_count_usage{0};
    //std::uint64_t               f_maximum_update_count = 0;
    //std::uint64_t               f_maximum_touch_count = 0;
    //std::uint64_t               f_maximum_allocated = 0;
    snapdev::preset_atomic<biased_time_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_first_item_created_on{biased_time_t()};
    snapdev::preset_atomic<biased_time_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_last_item_updated_on{biased_time_t()};
    snapdev::preset_atomic<biased_time_t, std::memory_order::relaxed, std::memory_order::relaxed>
                                f_last_item_touched_on{biased_time_t()};
    biased_time_t               f_most_recent_item_update = biased_time_t();
    std::size_t                 f_purge_index = 0;
    std::size_t                 f_decay_index = 0;
    std::size_t                 f_evict_stats_index = 0;
    std::size_t                 f_evict_stats_offset = 0;
    biased_time_t               f_evict_stat_last_decayed_on = biased_time_t(); // to apply various decay to item counters
    double                      f_evict_stat_update_decay_frac = 0.0;
    double                      f_evict_stat_touch_decay_frac = 0.0;
    std::size_t                 f_evict_weight_index = 0;
    std::size_t                 f_evict_weight_offset = 0;
    std::size_t                 f_evict_smallest_weights_count = 0;
    smallest_evict_item_t       f_evict_smallest_weights = smallest_evict_item_t();
    organized_evict_items_t     f_evict_targets_by_bucket = organized_evict_items_t();
    std::once_flag              f_cleanup_thread_started = std::once_flag();

    // statistics; place each counter within its own cache line
    // so a get() does not disturb a set() and vice versa
    //
    snapdev::preset_atomic<std::uint64_t, std::memory_order::relaxed, std::memory_order::relaxed>
    //mutable std::atomic<std::uint64_t>
                                f_statistics[static_cast<std::size_t>(statistic_counter_t::STATISTIC_COUNTER_max)] = {};
};



} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
