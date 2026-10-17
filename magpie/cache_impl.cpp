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
 * \brief The core implementation of the magpie cache.
 *
 * This file implements the core functions of the magpie cache functionality.
 */


// self
//
#include    <magpie/cache_impl.h>

#include    <magpie/cache.h>
#include    <magpie/cleanup_runner.h>
#include    <magpie/lock_mutex.h>
#include    <magpie/lock_recursive_rwlock.h>
#include    <magpie/lock_rwlock.h>



// cppthread
//
#include    <cppthread/thread.h>


// murmur3
//
#include    <murmur3/murmur3.h>


// C++
//
#include    <cstring>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{
namespace detail
{



cleanup_runner::pointer_t       g_cleanup_runner = cleanup_runner::pointer_t();


/** \brief Thread used to clean up in the background.
 *
 * This thread is used to do clean ups of the different caches in the
 * background.
 *
 * \todo
 * For now, I use just one thread. Later we may want to consider using
 * coroutines instead, one per cache. Or use our worker pool, but then
 * we need to implement some form of time based wake up call to some
 * thread in the pool...
 */
cppthread::thread::pointer_t    g_cleanup_thread = cppthread::thread::pointer_t();


time_t const                    g_start_time = time(nullptr) - 1; // -1 to make sure we do not divide by zero


/** \brief Start or join the cleanup worker thread.
 *
 * When a cache_impl pointer is specified in \p impl, then this
 * function makes sure to start the cleanup worker thread and
 * assign that cache as one of the caches to take care of.
 *
 * Each cache_impl calls this function exactly once after it
 * determined that it needs a cleanup thread.
 *
 * When the cache gets destroyed, it calls the cache_impl::done()
 * function which, in turn, calls this function to join the thread
 * if that was the last cache_impl being removed.
 *
 * \param[in] impl  The cache_impl to manage. If nullptr, we are trying
 * to join the thread.
 * \param[in] done  Whether the cache_impl is done or not.
 */
void start_stop_cleanup_worker(cache_impl::pointer_t impl, bool done)
{
    static cppthread::mutex local_lock = cppthread::mutex();
    cppthread::guard lock(local_lock);

    // done with the thread?
    //
    if(done)
    {
        if(g_cleanup_thread == nullptr)
        {
            // thread was never allocated, nothing to do here
            //
std::cout << gettid() << ": --- start_stop_cleanup_worker() no cleanup thread? ---" << std::endl;
            return;
        }
std::cout << gettid() << ": --- start_stop_cleanup_worker() ready to release cache ---" << std::endl;
        g_cleanup_runner->release_cache(impl);

        if(!g_cleanup_runner->all_caches_removed())
        {
            // the runner still has some cache_impl to manage
            //
std::cout << gettid() << ": --- start_stop_cleanup_worker() still has caches to manage ---" << std::endl;
            return;
        }
std::cout << gettid() << ": --- start_stop_cleanup_worker() done managing threads ---" << std::endl;
        //g_cleanup_thread->stop([](cppthread::thread * t){
        //        static_cast<cleanup_runner *>(t->get_runner())->wakeup();
        //    });
        g_cleanup_thread.reset();
        g_cleanup_runner.reset();
    }
    else
    {
        if(g_cleanup_thread == nullptr)
        {
            g_cleanup_runner = std::make_shared<cleanup_runner>();
            g_cleanup_thread = std::make_shared<cppthread::thread>("cleanup_thread", g_cleanup_runner.get());
        }

        if(!g_cleanup_thread->is_running())
        {
            // the first time, the f_started is false, so stop() does nothing
            // the following times, it joins the thread
            //
            g_cleanup_thread->stop();
            g_cleanup_thread->start();
        }

std::cout << gettid() << ": --- start_stop_cleanup_worker() add impl " << reinterpret_cast<void const *>(impl.get()) << " to thread ---" << std::endl;
        g_cleanup_runner->manage_cache(impl);
    }
}


cache_impl::bucket_t::bucket_t(lock_model_t lock_model)
{
    switch(lock_model)
    {
    case lock_model_t::LOCK_MODEL_MUTEX:
        f_lock = std::make_shared<lock_mutex>();
        break;

    case lock_model_t::LOCK_MODEL_RWLOCK:
        f_lock = std::make_shared<lock_rwlock>();
        break;

    case lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK:
        f_lock = std::make_shared<lock_recursive_rwlock>();
        break;

    default:
        throw not_implemented("unknown lock model.");

    }
}



cache_impl::cache_impl(
          cache_interface * parent
        , std::size_t bucket_count
        , lock_model_t lock_model)
    : f_parent(parent)
{
    f_buckets.reserve(bucket_count);
    for(std::size_t idx(0); idx < bucket_count; ++idx)
    {
        f_buckets.emplace_back(lock_model);
    }
}



hash_t cache_impl::hash_key(void const * key, std::size_t size)
{
    hash_t hash;
    MurmurHash3_x64_128_128(key, size, KEY_SEED1, KEY_SEED2, &hash);
    return hash;
}


void cache_impl::set_least_recently_used()
{
    f_least_recently_used = true;
}


void cache_impl::set_least_frequently_used()
{
    f_least_frequently_used = true;
}


void cache_impl::set_capacity(std::size_t capacity)
{
    f_capacity_limit = capacity;
}


void cache_impl::set_count(std::size_t count)
{
    f_count_limit = count;
}


void cache_impl::set_default_ttl(duration_t ttl)
{
    f_default_ttl = ttl;
}


void cache_impl::set_maximum_lifetime(duration_t max)
{
    f_maximum_lifetime = max;
}


void cache_impl::set_cleanup_interval(duration_t interval)
{
    f_cleanup_interval = interval;
}


duration_t cache_impl::get_cleanup_interval() const
{
    return f_cleanup_interval;
}


void cache_impl::set_wait_lock_for_stale_data_deletion(timespec const & max_wait)
{
    f_wait_lock_for_stale_data_deletion = max_wait;
}


void cache_impl::set_stale_data(stale_data_t mode)
{
    f_stale_data = mode;
}


/** \brief Determine whether the cleanup worker thread is mandatory.
 *
 * This function checks the cache parameters and returns true if the
 * function has parameters that mean the cleanup worker is likely
 * necessary.
 *
 * At the moment, this means:
 *
 * \li The capacity parameter is not zero.
 * \li The count parameter is not zero.
 * \li The default TTL is not NO_TTL and DEFAULT_TTL.
 * \li The maximum lifetime is not NO_MAXIMUM_LIFETIME.
 *
 * Note that if the default TTL shows as not turned off, the user may
 * still call set() with the TTL set to NO_TTL each time. In other words,
 * mark all the items as being cached forever anyway and in that case
 * the background clean up worker is not required. However, it is rather
 * unlikely so we do ignore that very special case.
 *
 * \return true if the function determines that the background worker
 * thread will be required.
 */
bool cache_impl::is_cleanup_worker_required() const
{
    return (f_default_ttl != NO_TTL && f_default_ttl != DEFAULT_TTL)
        || f_maximum_lifetime != NO_MAXIMUM_LIFETIME
        || f_capacity_limit > 0
        || f_count_limit > 0;
}


void cache_impl::start_cleanup_worker()
{
    std::call_once(
          f_thread_started
        , &start_stop_cleanup_worker
        , shared_from_this()
        , false);
}


void cache_impl::cache_done()
{
std::cout << gettid() << ": --- cache_done() called... calling stop ---" << std::endl;
    start_stop_cleanup_worker(shared_from_this(), true);
}


std::size_t cache_impl::size() const
{
#ifdef _DEBUG
    // verify that our counter is indeed current
    //
    std::size_t size(0);

    for(std::size_t idx(0); idx < f_buckets.size(); ++idx)
    {
        bucket_t & bucket{ const_cast<bucket_t &>(f_buckets[idx]) };

        // -------------------- RLOCK --------------------
        lock_guard lock(bucket.f_lock, false);

        size += bucket.f_entries.size();
    }

    if(size != f_current_count_usage)
    {
        throw logic_error("current real size() and f_current_count_usage do not match");
    }
#endif

    return f_current_count_usage;
}


bool cache_impl::is_limit_count_reached() const
{
    return f_count_limit != 0 && size() >= f_count_limit;
}


/** \brief Function used to compute the amount of memory allocated.
 *
 * This function attempts to determine the amount of memory allocated
 * by the elements in the cache. This is always short because it does
 * not count the size of the structures used to maintain the cache.
 *
 * \warning
 * This function is available only if your T class (as defined in the
 * cache) supports a size() function.
 *
 * \return The total number of bytes allocated at the moment.
 */
std::size_t cache_impl::allocated() const
{
#ifdef _DEBUG
    std::size_t size(0);

    for(std::size_t idx(0); idx < f_buckets.size(); ++idx)
    {
        bucket_t & bucket{ const_cast<bucket_t &>(f_buckets[idx]) };

        // -------------------- RLOCK --------------------
        lock_guard lock(bucket.f_lock, false);

        for(auto const & e : bucket.f_entries)
        {
            size += f_parent->allocated(e.second.f_value, e.second.f_size);
        }
    }

    if(size != f_current_capacity_usage)
    {
        throw logic_error("current capacity() and f_current_capacity_usage do not match");
    }
#endif

    return f_current_capacity_usage;
}


bool cache_impl::is_capacity_reached() const
{
    return f_capacity_limit != 0 && allocated() >= f_capacity_limit;
}


bool cache_impl::item_timed_out(item_t const & item) const
{
    time_t const now(time(nullptr));
    return (item.f_ttl != NO_TTL && item.f_ttl != DEFAULT_TTL && now >= item.f_last_access + item.f_ttl)
        || (f_maximum_lifetime != NO_MAXIMUM_LIFETIME && now >= item.f_created_on + f_maximum_lifetime);
}


std::size_t cache_impl::get_bucket_index(hash_t h) const
{
    return h % f_buckets.size();
}


void cache_impl::release()
{
    // before we can delete a cache_impl, we must make sure
    // it is not currently defined in the cleanup thread list
    //
}


bool cache_impl::set(
      hash_t h
    , void const * value
    , std::size_t value_size
    , duration_t ttl)
{
    time_t const now(time(nullptr));

    // TODO: this constructor means we get TWO copies of the object for new items
    //
    item_t item(f_parent, value, value_size);
    item.f_created_on = now;
    item.f_last_access = now;
    item.f_ttl = ttl == DEFAULT_TTL ? f_default_ttl : ttl;

    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ f_buckets[index] };

    // -------------------- LOCK --------------------
    lock_guard lock(bucket.f_lock, true);

    if(ttl != DEFAULT_TTL && ttl != NO_TTL && ttl > 0)
    {
        start_cleanup_worker();
    }

    auto it(bucket.f_entries.find(h));
    if(it == bucket.f_entries.end())
    {
        bucket.f_entries.emplace(h, item);

        ++f_current_count_usage;
    }
    else
    {
        if(item.f_size != it->second.f_size)
        {
            throw logic_error("value buffer size mismatch in set() function."); // LCOV_EXCL_LINE
        }
        if(item.f_parent != it->second.f_parent)
        {
            throw logic_error("value buffer parent mismatch in set() function."); // LCOV_EXCL_LINE
        }

        f_current_capacity_usage -= it->second.f_allocated;

        std::swap(it->second.f_value, item.f_value);
        ++it->second.f_accesses;
        it->second.f_allocated = item.f_allocated;
        it->second.f_last_access = item.f_last_access;
        it->second.f_ttl = item.f_ttl;
    }

    f_current_capacity_usage += item.f_allocated;

    return true;
}


bool cache_impl::get(
      hash_t h
    , void * value
    , std::size_t value_size
    , bool update_ttl)
{
    time_t const now(time(nullptr));

    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ f_buckets[index] };

    // -------------------- RLOCK --------------------
    lock_guard shared_lock(bucket.f_lock, false);

    auto it(bucket.f_entries.find(h));
    if(it == bucket.f_entries.end())
    {
        // not in the cache
        //
        return false;
    }

    if(value_size != it->second.f_size)
    {
        throw logic_error("value buffer size mismatch in get() function."); // LCOV_EXCL_LINE
    }

    if(f_stale_data != stale_data_t::STALE_DATA_RETURN
    && item_timed_out(it->second))
    {
        if(f_stale_data == stale_data_t::STALE_DATA_DELETE)
        {
            // item timed out, get rid of it
            //
            if(bucket.f_lock->supports_rlock())
            {
                // in this case, the read-only lock is in place
                // we need to switch to the exclusive lock instead
                //
                shared_lock.unlock();

                // -------------------- LOCK --------------------
                lock_guard exclusive_lock(bucket.f_lock, f_wait_lock_for_stale_data_deletion, true);
                if(exclusive_lock.is_locked())
                {
                    --f_current_count_usage;
                    f_current_capacity_usage -= it->second.f_allocated;
                    bucket.f_entries.erase(it);
                }
                // TODO: else { send to clean up thread }
            }
            else
            {
                --f_current_count_usage;
                f_current_capacity_usage -= it->second.f_allocated;
                bucket.f_entries.erase(it);
            }
        }
        return false;
    }

    if(!update_ttl)
    {
        // "not updating ttl" means keeping the same deadline
        // which moves whenever the f_last_access parameter changes
        // so in this case we adjust the TTL to compensate
        //
        duration_t const diff(now - it->second.f_last_access);
        it->second.f_ttl = diff >= it->second.f_ttl
                            ? 0
                            : it->second.f_ttl - diff;
    }

    it->second.f_last_access = now;

    f_parent->copy(it->second.f_value, value, value_size);

    return true;
}


metadata_t cache_impl::get_metadata(hash_t h)
{
    metadata_t result = {};
    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ f_buckets[index] };

    // -------------------- RLOCK --------------------
    lock_guard lock(bucket.f_lock, false);

    auto it(bucket.f_entries.find(h));
    if(it == bucket.f_entries.end())
    {
        return result;
    }

    result.f_size = it->second.f_size;
    result.f_allocated = it->second.f_allocated;
    result.f_accesses = it->second.f_accesses;
    result.f_created_on = it->second.f_created_on;
    result.f_last_access = it->second.f_last_access;
    result.f_ttl = it->second.f_ttl;

    // compute the time when the data is considered stale
    //
    if(it->second.f_ttl != NO_TTL
    && it->second.f_ttl != DEFAULT_TTL)
    {
        result.f_stale_at = it->second.f_last_access + it->second.f_ttl;
    }
    if(f_maximum_lifetime != NO_MAXIMUM_LIFETIME)
    {
        time_t const maximum_stale_at(it->second.f_created_on + f_maximum_lifetime);
        result.f_stale_at = std::min(result.f_stale_at, maximum_stale_at);
    }

    return result;
}


bool cache_impl::erase(hash_t h)
{
    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ f_buckets[index] };

    // -------------------- LOCK --------------------
    lock_guard lock(bucket.f_lock, true);
    auto it(bucket.f_entries.find(h));
    if(it == bucket.f_entries.end())
    {
        return false;
    }
    --f_current_count_usage;
    f_current_capacity_usage -= it->second.f_allocated;
    bucket.f_entries.erase(it);
    return true;
}


bool cache_impl::contains(hash_t h) const
{
    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ const_cast<bucket_t &>(f_buckets[index]) };

    // -------------------- RLOCK --------------------
    lock_guard lock(bucket.f_lock, false);
    return bucket.f_entries.contains(h);
}


/** \brief Remove expired entries from one bucket.
 *
 * This function goes through all the entries of one bucket and erases
 * all those that have timed out.
 *
 * The function only handles one bucket at a time. Once all the buckets
 * were handled, it returns true. At that point, it is ready to process
 * another all buckets from the start again.
 *
 * \return true if the last bucket was processed.
 */
bool cache_impl::purge_expired()
{
    bucket_t & bucket{ f_buckets[f_purge_index] };

    {
        // -------------------- LOCK --------------------
        lock_guard lock(bucket.f_lock, true);

        auto it(bucket.f_entries.begin());
        while(it != bucket.f_entries.end())
        {
            if(item_timed_out(it->second))
            {
                --f_current_count_usage;
                f_current_capacity_usage -= it->second.f_allocated;
                it = bucket.f_entries.erase(it);
                ++f_purged_counter;
            }
            else
            {
                ++it;
            }
        }
    }

    ++f_purge_index;
    if(f_purge_index >= f_buckets.size())
    {
        f_purge_index = 0;
        return true;
    }

    return false;
}


/** \brief This function computes a weight for each item.
 *
 * The function goes through the cache and assigns weights to each item.
 * This is done "in realtime" while the cache is live so the weights
 * computed first will be wrong by the time we finish up computing all
 * the weights... but we still use what we've got.
 *
 * The idea is fairly simple. If some items need to be erased, then we
 * do need to go through the entire cache, which can be huge, and compute
 * that weight the best we can.
 *
 * There are several parameters that are used to compute the weight:
 *
 * * number of accesses / max. number of accesses
 * * item time of last access / time of last access through the entire cache
 *
 * There should be more available such as the cost of transferring data
 * over a network divided by the size of the object. i.e. is it best to
 * keep that item in memory compared to re-downloading it from the source.
 */
bool cache_impl::evict_stats()
{
    // if no LRU/LFU evictions are possible, then return "done" immediately
    //
    if(f_capacity_limit == 0
    && f_count_limit == 0)
    {
        return true;
    }

    if(f_evict_stats_index == 0)
    {
        // reset the statistic variables
        //
        f_evict_smallest_weights.clear();

        f_evict_stat_max_accesses_for_weight = f_evict_stat_max_accesses;
        f_evict_stat_latest_access_age_for_weight = time(nullptr) - g_start_time;

        f_evict_stat_max_accesses = 1;

        f_evict_stats_offset = snapdev::random(0UL, f_buckets.size() - 1UL);

        // setup a default weight count
        //
        f_evict_smallest_weights_count = EVICT_SMALLEST_WEIGHT_COUNT;

        // compute the number of items we want to collect to reduce the size
        // from the current size to the reduction percent if the count trigger
        // is reached
        //
        double const current_size(size());
        double const eviction_trigger(f_count_limit * f_size_eviction_trigger_percent);
        if(current_size >= eviction_trigger)
        {
            // add a little bit so by the time we start evicting we
            // get at least enough entries
            //
            double const eviction_count(f_count_limit * f_size_eviction_reduction_percent + 10.0);
            if(eviction_count > f_evict_smallest_weights_count)
            {
                f_evict_smallest_weights_count = eviction_count;
            }
        }
    }

    // the following allows us to walk all the buckets in a seemingly
    // random manner without having to shuffle a large number of indexes
    // 
    // as a result, this means we do not give one bucket a higher priority
    // over long period of time
    //
    std::size_t const index((f_evict_stats_index * EVICT_SWEEP_STRIDE + f_evict_stats_offset) % f_buckets.size());
    bucket_t & bucket{ f_buckets[index] };

    lock_guard lock(bucket.f_lock, false);

    for(auto & e : bucket.f_entries)
    {
        if(e.second.f_accesses > f_evict_stat_max_accesses)
        {
            f_evict_stat_max_accesses = e.second.f_accesses;
        }

        double weight(1.0);

        // compute our weights
        //
        weight *= e.second.f_accesses / f_evict_stat_max_accesses_for_weight;
        weight *= (e.second.f_last_access - g_start_time) / f_evict_stat_latest_access_age_for_weight;

        std::size_t const allocated(f_parent->allocated(e.second.f_value, e.second.f_size));

        if(f_evict_smallest_weights.size() < f_evict_smallest_weights_count)
        {
            f_evict_smallest_weights.emplace(e.first, weight, allocated);
        }
        else if(weight < f_evict_smallest_weights.rbegin()->f_weight)
        {
            f_evict_smallest_weights.erase(std::prev(f_evict_smallest_weights.end()));
            f_evict_smallest_weights.emplace(e.first, weight, allocated);
        }
    }

    ++f_evict_stats_index;
    if(f_evict_stats_index >= f_buckets.size())
    {
        f_evict_stats_index = 0;
        return true;
    }

    return false;
}


bool cache_impl::evict_organize_by_bucket()
{
    std::size_t current_count(size());
    std::size_t current_allocated(allocated());

    std::size_t count_limit(f_count_limit * f_size_eviction_reduction_percent);
    std::size_t capacity_limit(f_capacity_limit * f_size_eviction_reduction_percent);

    auto need_more_evictions([count_limit, capacity_limit, &current_count, &current_allocated]()
        {
            return current_count > count_limit
                || current_allocated > capacity_limit;
        });

    if(!need_more_evictions())
    {
        return true;
    }

    for(auto const & e : f_evict_smallest_weights)
    {
        if(!need_more_evictions())
        {
            break;
        }

        std::size_t const bucket_idx(static_cast<std::uint64_t>(e.f_hash) % f_buckets.size());
        f_targets_by_bucket[bucket_idx].push_back(e.f_hash);

        --current_count;
        current_allocated -= e.f_allocated;
    }

    return true;
}


/** \brief Evict various items from the cache.
 *
 * The evict_stats() function generates a set of items to be evicted.
 * This function processes that set by erasing those items. It processes
 * all the items of a specific bucket then returns. If the set of items
 * is empty, the number of items is low enough, or the capacity went down
 * to a proper level, then the function returns true (done).
 *
 * \return true if the eviction process is considered done.
 */
bool cache_impl::evict()
{
    if(!f_targets_by_bucket.empty())
    {
        auto bucket_it(f_targets_by_bucket.begin());
        std::size_t const bucket_idx(bucket_it->first);
        bucket_t & bucket{ f_buckets[bucket_idx] };

        // -------------------- LOCK --------------------
        lock_guard lock(bucket.f_lock, true);

        for(auto & h : bucket_it->second)
        {
            // WARNING: at the moment, the locks are considered non-recursive
            //          because the rwlock is not--so calling erase(h) from
            //          here would end up in a deadlock
            //
            auto it(bucket.f_entries.find(h));
            if(it == bucket.f_entries.end())
            {
                return false;
            }
            --f_current_count_usage;
            f_current_capacity_usage -= it->second.f_allocated;
            bucket.f_entries.erase(it);
        }
        f_targets_by_bucket.erase(f_targets_by_bucket.begin());
    }

    return f_targets_by_bucket.empty();
}


lock_interface::pointer_t cache_impl::get_lock(hash_t h)
{
    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ const_cast<bucket_t &>(f_buckets[index]) };
    return bucket.f_lock;
}



cache_impl::item_t::item_t(
          cache_interface * parent
        , void const * value
        , std::size_t size)
    : f_parent(parent)
    , f_size(size)
{
    f_value = malloc(f_size);
    if(f_value == nullptr)
    {
        throw std::bad_alloc();
    }
    f_parent->copy(value, f_value, f_size);
    f_allocated = f_parent->allocated(f_value, f_size);
}


cache_impl::item_t::item_t(item_t const & rhs)
    : f_parent(rhs.f_parent)
    , f_size(rhs.f_size)
    , f_allocated(rhs.f_allocated)
    , f_accesses(rhs.f_accesses)
    , f_created_on(rhs.f_created_on)
    , f_last_access(rhs.f_last_access)
    , f_ttl(rhs.f_ttl)
{
    f_value = malloc(f_size);
    if(f_value == nullptr)
    {
        throw std::bad_alloc();
    }
    f_parent->copy(rhs.f_value, f_value, f_size);
}


cache_impl::item_t::~item_t()
{
    f_parent->destroy(f_value, f_size);
    free(f_value);
}


cache_impl::item_t const & cache_impl::item_t::operator = (item_t const & rhs)
{
    if(this != &rhs)
    {
        if(f_parent != rhs.f_parent)
        {
            throw logic_error("all the items in the cache must have the exact same parent.");
        }
        if(f_size != rhs.f_size)
        {
            throw logic_error("all the items in the cache must have the exact same size.");
        }

        f_parent->destroy(f_value, f_size);
        f_parent->copy(rhs.f_value, f_value, f_size);

        f_allocated = rhs.f_allocated;
        f_accesses = rhs.f_accesses;
        f_created_on = rhs.f_created_on;
        f_last_access = rhs.f_last_access;
        f_ttl = rhs.f_ttl;
    }

    return *this;
}



} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
