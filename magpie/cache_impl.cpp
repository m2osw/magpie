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
#include    <magpie/utils.h>



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


enum class cleanup_worker_action_t
{
    CLEANUP_WORKER_ACTION_START,
    CLEANUP_WORKER_ACTION_STOP,
    CLEANUP_WORKER_ACTION_WAKEUP,
};



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
void start_stop_cleanup_worker(cache_impl::pointer_t impl, cleanup_worker_action_t action)
{
    static cppthread::mutex local_lock = cppthread::mutex();
    cppthread::guard lock(local_lock);
std::cerr << gettid() << ": --- start_stop_cleanup_worker() get cleanup thread ready or stopped ---" << std::endl;

    // done with the thread?
    //
    switch(action)
    {
    case cleanup_worker_action_t::CLEANUP_WORKER_ACTION_START:
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

        g_cleanup_runner->manage_cache(impl);
        break;

    case cleanup_worker_action_t::CLEANUP_WORKER_ACTION_STOP:
        if(g_cleanup_thread == nullptr)
        {
            // thread was never allocated, nothing to do here
            //
std::cerr << gettid() << ": --- start_stop_cleanup_worker() no cleanup thread? ---" << std::endl;
            break;
        }
std::cerr << gettid() << ": --- start_stop_cleanup_worker() ready to release cache ---" << std::endl;
        g_cleanup_runner->release_cache(impl);

        if(!g_cleanup_runner->all_caches_removed())
        {
            // the runner still has some cache_impl to manage
            //
std::cerr << gettid() << ": --- start_stop_cleanup_worker() still has caches to manage ---" << std::endl;
            break;
        }
std::cerr << gettid() << ": --- start_stop_cleanup_worker() done managing threads ---" << std::endl;
        g_cleanup_thread.reset();
        g_cleanup_runner.reset();
        break;

    case cleanup_worker_action_t::CLEANUP_WORKER_ACTION_WAKEUP:
        if(g_cleanup_thread == nullptr)
        {
            // nothing to wake up
            //
            break;
        }
        static_cast<cleanup_runner *>(g_cleanup_runner.get())->wakeup();
        break;

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

    default:                                            // LCOV_EXCL_LINE
        throw not_implemented("unknown lock model.");   // LCOV_EXCL_LINE

    }
}






cache_impl::item_t::item_t(cache_interface * parent)
    : f_parent(parent)
{
}


cache_impl::item_t::item_t(item_t const & rhs)
    : f_parent(rhs.f_parent)
    , f_size(rhs.f_size)
    , f_created_on(rhs.f_created_on)
    , f_value(nullptr)
    , f_allocated(rhs.f_allocated)
    , f_ttl(rhs.f_ttl)
    , f_last_updated_on(rhs.f_last_updated_on)
    , f_access_count(rhs.f_access_count)
    , f_last_touched_on(rhs.f_last_touched_on)
    , f_timeout(rhs.f_timeout)
    , f_turn(rhs.f_turn)
{
    if(rhs.f_size != 0
    && rhs.f_value != nullptr)
    {
        f_value = malloc(rhs.f_size);
        if(f_value == nullptr)
        {
            throw std::bad_alloc(); // LCOV_EXCL_LINE
        }

        try
        {
            f_parent->copy(rhs.f_value, f_value, f_size);
        }
        catch(...)
        {
            free(f_value);
            f_value = nullptr;
            f_size = 0;
            throw;
        }
    }
}


cache_impl::item_t::~item_t()
{
    if(f_value != nullptr)
    {
        f_parent->destroy(f_value, f_size);
        free(f_value);
    }
}


cache_impl::item_t const & cache_impl::item_t::operator = (item_t const & rhs)
{
    if(this != &rhs)
    {
        if(rhs.f_size != 0
        && rhs.f_value != nullptr)
        {
            if(f_parent != rhs.f_parent)
            {
                throw logic_error("all the items in the cache must have the exact same parent."); // LCOV_EXCL_LINE
            }
            if(f_size != 0
            && f_size != rhs.f_size)
            {
                throw logic_error("all the items in the cache must have the exact same size."); // LCOV_EXCL_LINE
            }

            if(f_value != nullptr)
            {
                f_parent->destroy(f_value, f_size);
            }
            else
            {
                f_value = malloc(rhs.f_size);
                if(f_value == nullptr)
                {
                    throw std::bad_alloc(); // LCOV_EXCL_LINE
                }
                f_size = rhs.f_size;
            }
            try
            {
                f_parent->copy(rhs.f_value, f_value, f_size);
            }
            catch(...)
            {
                free(f_value);
                f_value = nullptr;
                f_size = 0;
            }
        }
        else if(f_value != nullptr)
        {
            free(f_value);
            f_value = nullptr;
            f_size = 0;
        }

        f_created_on = rhs.f_created_on;

        f_allocated = rhs.f_allocated;
        f_ttl = rhs.f_ttl;
        f_last_updated_on = rhs.f_last_updated_on;

        f_access_count = rhs.f_access_count;
        f_last_touched_on = rhs.f_last_touched_on;
        f_timeout = rhs.f_timeout;
        f_turn = rhs.f_turn;
    }

    return *this;
}


void cache_impl::item_t::set_value(void const * value, std::size_t size)
{
    // the size is not expected to change, if 0 the user is using
    // the cache as a set (Set Cache, Presence Cache, Filter Cache)
    //
    if(size == 0)
    {
        if(f_size != 0)
        {
            throw logic_error("a value exists in item_t, but it has a different size (1)."); // LCOV_EXCL_LINE
        }
        return;
    }

    if(f_value == nullptr)
    {
        f_value = malloc(size);
        if(f_value == nullptr)
        {
            throw std::bad_alloc(); // LCOV_EXCL_LINE
        }
        f_size = size;
    }
    else
    {
        if(f_size != size)
        {
            throw logic_error("a value exists in item_t, but it has a different size (2)."); // LCOV_EXCL_LINE
        }
        f_parent->destroy(f_value, f_size);
    }
    f_parent->copy(value, f_value, f_size);
}


snapdev::timespec_ex cache_impl::item_t::last_accessed_on() const
{
    return std::max(f_last_updated_on, f_last_touched_on);
}










cache_impl::cache_impl(
          cache_interface * parent
        , options const & opts)
    : f_parent(parent)
    , f_opts(opts)
{
    std::size_t const bucket_count(opts.get_bucket_count());
    lock_model_t const lock_model(opts.get_lock_model());

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


duration_t cache_impl::get_cleanup_interval() const
{
    return f_opts.get_cleanup_interval();
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
    return (f_opts.get_default_ttl() != NO_TTL && f_opts.get_default_ttl() != DEFAULT_TTL)
        || f_opts.get_maximum_lifetime() != NO_MAXIMUM_LIFETIME
        || f_opts.get_capacity_limit() > 0
        || f_opts.get_count_limit() > 0;
}


void cache_impl::start_cleanup_worker()
{
std::cerr << gettid() << ": --- start cleanup worker now... ---" << std::endl;
    std::call_once(
          f_cleanup_thread_started
        , &start_stop_cleanup_worker
        , shared_from_this()
        , cleanup_worker_action_t::CLEANUP_WORKER_ACTION_START);
}


void cache_impl::cache_done()
{
std::cerr << gettid() << ": --- cache_done() called... calling stop ---" << std::endl;
    start_stop_cleanup_worker(shared_from_this(), cleanup_worker_action_t::CLEANUP_WORKER_ACTION_STOP);
}


void cache_impl::activate_eviction()
{
std::cerr << gettid() << ": --- cache_done() called... calling stop ---" << std::endl;
    start_stop_cleanup_worker(shared_from_this(), cleanup_worker_action_t::CLEANUP_WORKER_ACTION_WAKEUP);
}


#ifdef _DEBUG
void cache_impl::verify_current_count()
{
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
        throw logic_error("current real size() and f_current_count_usage do not match"); // LCOV_EXCL_LINE
    }
}


void cache_impl::verify_current_capacity()
{
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
        throw logic_error("current capacity() and f_current_capacity_usage do not match"); // LCOV_EXCL_LINE
    }
}
#endif


std::size_t cache_impl::size() const
{
    return f_current_count_usage;
}


bool cache_impl::is_limit_count_reached(bool exact) const
{
    // limit defined?
    //
    std::size_t eviction_trigger(f_opts.get_count_limit());
    if(eviction_trigger == 0)
    {
        return false;
    }

    // if we have a policy, check it to know when we reached the
    // limit (the trigger percentage)
    //
    if(!exact)
    {
        eviction_policy::pointer_t ep(f_opts.get_eviction_policy());
        if(ep != nullptr)
        {
            eviction_trigger = ep->get_size_eviction_trigger_limit(eviction_trigger);
        }
        else
        {
            eviction_trigger = static_cast<std::size_t>(floor(eviction_trigger * EVICTION_TRIGGER_DEFAULT));
        }
    }

    return size() >= eviction_trigger;
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
    return f_current_capacity_usage;
}


bool cache_impl::is_capacity_reached(bool exact) const
{
    // limit defined?
    //
    std::size_t eviction_trigger(f_opts.get_capacity_limit());
    if(eviction_trigger == 0)
    {
        return false;
    }

    // if we have a policy, check it to know when we reached the
    // limit (the trigger percentage)
    //
    if(!exact)
    {
        eviction_policy::pointer_t ep(f_opts.get_eviction_policy());
        if(ep != nullptr)
        {
            eviction_trigger = ep->get_size_eviction_trigger_limit(eviction_trigger);
        }
        else
        {
            eviction_trigger = static_cast<std::size_t>(floor(eviction_trigger * EVICTION_TRIGGER_DEFAULT));
        }
    }

    return allocated() >= eviction_trigger;
}


bool cache_impl::item_timed_out(item_t const & item) const
{
    snapdev::timespec_ex const now(time(nullptr));
    return (item.f_timeout != snapdev::timespec_ex() && now >= item.f_timeout)
        || (f_opts.get_maximum_lifetime() != NO_MAXIMUM_LIFETIME && now >= item.f_created_on + f_opts.get_maximum_lifetime());
}


std::size_t cache_impl::get_bucket_index(hash_t h) const
{
    return static_cast<std::uint64_t>(h) % f_buckets.size();
}


set_result_t cache_impl::set(
      hash_t h
    , void const * value
    , std::size_t value_size
    , duration_t ttl)
{
    snapdev::timespec_ex const now(snapdev::now());

    if(ttl == DEFAULT_TTL)
    {
        ttl = f_opts.get_default_ttl();
    }
    if(ttl != DEFAULT_TTL && ttl != NO_TTL)
    {
        if(ttl <= 0)
        {
            erase(h);
            return set_result_t::SET_RESULT_REJECTED_EXPIRED;
        }
        if(f_opts.get_default_ttl() != DEFAULT_TTL && f_opts.get_default_ttl() != NO_TTL)
        {
            // choose between at least 5 min. or 1% of the default TTL
            //
            double const min_ttl(std::min(300.0, f_opts.get_default_ttl() * 0.01));
            if(ttl < min_ttl)
            {
                // ttl is really short, avoid saving this value
                //
                erase(h);
                return set_result_t::SET_RESULT_REJECTED_EXPIRED;
            }
        }
    }

    std::size_t const allocated_bytes(f_parent->allocated(value, value_size));
    if(f_opts.get_maximum_object_size() != 0
    && allocated_bytes > f_opts.get_maximum_object_size())
    {
        erase(h);
        return set_result_t::SET_RESULT_REJECTED_TOO_LARGE;
    }

    // TODO: this constructor means we get TWO copies of the object for new items
    //
    item_t item(f_parent);
    item.f_allocated = allocated_bytes;
    item.f_created_on = now;
    item.f_last_updated_on = now;
    item.f_ttl = ttl;
    if(ttl > 0)
    {
        item.f_timeout = now + ttl;
    }

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
        // in this case the object is new so the count/size is additive
        //
        if(f_opts.get_refuse_data_once_full())
        {
            if(is_limit_count_reached(true))
            {
                activate_eviction();
                return set_result_t::SET_RESULT_REJECTED_COUNT;
            }

            if(is_capacity_reached(true))
            {
                activate_eviction();
                return set_result_t::SET_RESULT_REJECTED_CAPACITY;
            }
        }

        std::pair<item_t::map_t::iterator, bool> inserted(bucket.f_entries.emplace(h, item));
#ifdef _DEBUG
        if(!inserted.second)
        {
            throw logic_error("somehow emplace() did not insert the element even though we just checked and it was not there."); // LCOV_EXCL_LINE
        }
#endif
        inserted.first->second.set_value(value, value_size);

        if(f_first_item_created_on == snapdev::timespec_ex())
        {
            f_first_item_created_on = now;
        }
        f_last_item_updated_on = now;
        ++f_current_count_usage;
        f_current_capacity_usage += item.f_allocated;

        return set_result_t::SET_RESULT_ADDED;
    }
    else
    {
        if(value_size != it->second.f_size)
        {
            throw logic_error("item value size mismatch in set() function."); // LCOV_EXCL_LINE
        }
        if(item.f_parent != it->second.f_parent)
        {
            throw logic_error("item parent mismatch in set() function."); // LCOV_EXCL_LINE
        }

        // in this case the object is being updated so the count doesn't
        // change but the size may be larger and may require rejection
        //
        if(f_opts.get_refuse_data_once_full()
        && item.f_allocated > it->second.f_allocated
        && f_opts.get_capacity_limit() != 0)
        {
            std::size_t const additional(item.f_allocated - it->second.f_allocated);
            if(f_current_capacity_usage + additional >= f_opts.get_capacity_limit())
            {
                bucket.f_entries.erase(it);
                activate_eviction();
                return set_result_t::SET_RESULT_REJECTED_CAPACITY;
            }
        }

        f_current_capacity_usage -= it->second.f_allocated;
        f_current_capacity_usage += item.f_allocated;
        f_last_item_updated_on = now;

        it->second.set_value(value, value_size);
        it->second.f_allocated = item.f_allocated;
        it->second.f_ttl = item.f_ttl;

        ++it->second.f_access_count;
        if(it->second.f_access_count > f_maximum_access_count)
        {
            f_maximum_access_count = it->second.f_access_count;
        }
        if(it->second.f_allocated > f_maximum_allocated)
        {
            f_maximum_allocated = it->second.f_allocated;
        }
        it->second.f_last_updated_on = now;
        it->second.f_timeout = item.f_timeout;

        return set_result_t::SET_RESULT_UPDATED;
    }

    snapdev::NOT_REACHED();
}


bool cache_impl::get(
      hash_t h
    , void * value
    , std::size_t value_size
    , bool update_ttl)
{
    snapdev::timespec_ex const now(snapdev::now());

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

    if(f_opts.get_stale_data() != stale_data_t::STALE_DATA_RETURN
    && item_timed_out(it->second))
    {
        if(f_opts.get_stale_data() == stale_data_t::STALE_DATA_DELETE)
        {
std::cerr << "--- auto-purge in get()?!" << std::endl;

            // item timed out, get rid of it
            //
            if(bucket.f_lock->supports_rlock())
            {
                // in this case, the read-only lock is in place
                // we need to switch to the exclusive lock instead
                //
                shared_lock.unlock();

                // -------------------- LOCK --------------------
                lock_guard exclusive_lock(bucket.f_lock, f_opts.get_wait_lock_for_stale_data_deletion(), true);
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

    if(update_ttl)
    {
        // "updating ttl" means updating the f_timeout field to the
        // now + f_ttl -- i.e. it pushes back when the value is considered
        // stale
        //
        it->second.f_timeout = now + it->second.f_ttl;
    }

    it->second.f_last_touched_on = now;
    f_last_item_touched_on = now;

    std::size_t const max_access_count(f_opts.get_maximum_access_count());
    if(max_access_count == NO_LIMIT
    || it->second.f_access_count < max_access_count)
    {
        ++it->second.f_access_count;
        if(it->second.f_access_count > f_maximum_access_count)
        {
            f_maximum_access_count = it->second.f_access_count;
        }
    }

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
    result.f_access_count = it->second.f_access_count;
    result.f_created_on = it->second.f_created_on;
    result.f_last_updated_on = it->second.f_last_updated_on;
    result.f_last_touched_on = it->second.f_last_touched_on;
    result.f_ttl = it->second.f_ttl;
    result.f_timeout = it->second.f_timeout;

    // compute the time when the data is considered stale
    //
    result.f_stale_at = it->second.f_timeout;
    if(f_opts.get_maximum_lifetime() != NO_MAXIMUM_LIFETIME)
    {
        snapdev::timespec_ex const maximum_stale_at(it->second.f_created_on + f_opts.get_maximum_lifetime());
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
std::cerr << "--- purge_expired() called... " << f_current_count_usage << "/" << f_purged_counter << std::endl;
    bucket_t & bucket{ f_buckets[f_purge_index] };

    {
        // -------------------- LOCK --------------------
        lock_guard lock(bucket.f_lock, true);

        auto it(bucket.f_entries.begin());
        while(it != bucket.f_entries.end())
        {
//std::cerr << std::boolalpha << "--- purge_expired() >> bucket[" << f_purge_index << "] = "
//<< *reinterpret_cast<std::string const *>(it->second.f_value)
//<< " -- now: " << (time(nullptr))
//<< " vs timeout " << it->second.f_timeout
//<< " -> " << (it->second.f_timeout != time_t() && time(nullptr) >= it->second.f_timeout)
//<< " [" << (it->second.f_timeout != time_t()) << ", " << (time(nullptr) >= it->second.f_timeout) << "]"
//<< ".\n";
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
std::cerr << "--- purge_expired() done looping... " << f_current_count_usage << "/" << f_purged_counter << std::endl;

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
 * computed first are like to be wrong by the time we finish up
 * computing all the weights. To reduce the issue within the whole
 * cache, the order in which we check each bucket changes on each
 * cycle.
 *
 * The idea is simple. When items need to be evicted, this function
 * gets called and it starts computing weights for items within one
 * bucket. The items with the lowest weight are the one that get
 * evicted.
 *
 * The function handles many parameters through the eviction_policy
 * defined when creating the cache. If no eviction policy was defined,
 * then this way of evicting items is not used.
 *
 * All the possible parameters are defined in the eviction policy.
 * There are three categories:
 *
 * \li Parameters you defined when you initialize the policy
 *     (i.e. at what percentage do we start * eviction).
 * \li Parameters defined on each cycle; one cycle is the work
 *     of computing weights of all the buckets and performing
 *     the eviction (i.e. the timestamp when the oldest item
 *     was created).
 * \li Parameters defined for each item (i.e. the item access
 *     count).
 *
 * The weight computation is done by the eviction_policy::compute_weight()
 * function. You can actually control that computation 100% since
 * you are given the ability to create your own policy and pass it
 * to the options objects used to create the cache.
 *
 * The cycle mentioned above includes one call to this function per
 * non-empty bucket. If all buckets are currently empty, then the
 * function returns true immediately (although it should not get
 * called since we would not have reached the limits if empty).
 *
 * \return true if the eviction statistic computations are complete.
 */
bool cache_impl::evict_stats()
{
    // if no evictions policy defined, then return "done" immediately
    //
    eviction_policy::pointer_t ep(f_opts.get_eviction_policy());
    if(ep == nullptr)
    {
        return true;
    }

    if(f_evict_stats_index == 0)
    {
        f_evict_stat_started_on = time(nullptr);
        ep->set_cycle_started_on(f_evict_stat_started_on);

        // reset the variables before gathering their value from the
        // cached items
        //
        f_evict_stat_max_access_count = 1;
        f_evict_stat_max_allocated = 1;
        f_evict_stat_oldest_item = snapdev::timespec_ex::max();
        f_evict_stat_most_recent_item_update = f_first_item_created_on;
        f_evict_stat_most_recent_item_touch = f_last_item_touched_on;
        f_evict_stat_oldest_item_update = snapdev::timespec_ex::max();
        f_evict_stat_oldest_item_touch = snapdev::timespec_ex::max();
        f_evict_stat_highest_item_turn = 0;

        f_evict_stats_offset = snapdev::random(0UL, f_buckets.size() - 1UL);
    }

    // the EVICT_SWEEP_STRIDE and f_evict_stats_offset gives us a seemingly
    // random manner to go through the bucket without having to shuffle a
    // large number of indexes
    // 
    // as a result, this means we do not give one bucket a higher priority
    // over a long period of time
    //
    bool skip_empty(false);
    do
    {
        std::size_t const index((f_evict_stats_index * EVICT_SWEEP_STRIDE + f_evict_stats_offset) % f_buckets.size());
        bucket_t & bucket{ f_buckets[index] };

        {
            lock_guard lock(bucket.f_lock, false);

            skip_empty = bucket.f_entries.empty();
            for(auto & e : bucket.f_entries)
            {
                if(e.second.f_access_count > f_evict_stat_max_access_count)
                {
                    f_evict_stat_max_access_count = e.second.f_access_count;
                }
                if(e.second.f_allocated > f_evict_stat_max_allocated)
                {
                    f_evict_stat_max_allocated = e.second.f_allocated;
                }
                if(e.second.f_created_on < f_evict_stat_oldest_item)
                {
                    f_evict_stat_oldest_item = e.second.f_created_on;
                }
                if(e.second.f_last_updated_on > f_evict_stat_most_recent_item_update)
                {
                    f_evict_stat_most_recent_item_update = e.second.f_last_updated_on;
                }
                if(e.second.f_last_touched_on > f_evict_stat_most_recent_item_touch)
                {
                    f_evict_stat_most_recent_item_touch = e.second.f_last_touched_on;
                }
                if(e.second.f_last_updated_on < f_evict_stat_oldest_item_update)
                {
                    f_evict_stat_oldest_item_update = e.second.f_last_updated_on;
                }
                if(e.second.f_last_touched_on < f_evict_stat_oldest_item_touch)
                {
                    f_evict_stat_oldest_item_touch = e.second.f_last_touched_on;
                }
                if(e.second.f_turn < f_evict_stat_highest_item_turn)
                {
                    f_evict_stat_highest_item_turn = e.second.f_turn;
                }

                // this needs a better timer because when full, the cache
                // triggers an eviction which means this could happen very
                // often, more often than expected making the account count
                // reach zero quickly
                //
                std::uint64_t const decay(f_opts.get_access_count_decay());
                if(decay != NO_DECAY)
                {
                    if(e.second.f_access_count > decay)
                    {
                        e.second.f_access_count -= decay;
                    }
                    else
                    {
                        e.second.f_access_count = 0;
                    }
                }

                ++e.second.f_turn;
            }
        }

        ++f_evict_stats_index;
        if(f_evict_stats_index >= f_buckets.size())
        {
            f_evict_stats_index = 0;

            ep->set_maximum_access_count(f_evict_stat_max_access_count);
            ep->set_maximum_allocated(f_evict_stat_max_allocated);
            ep->set_oldest_item_created_on(f_evict_stat_oldest_item.to_sec());
            ep->set_most_recent_item_update(f_evict_stat_most_recent_item_update.to_sec());
            ep->set_most_recent_item_touch(f_evict_stat_most_recent_item_touch.to_sec());
            ep->set_oldest_item_update(f_evict_stat_oldest_item_update.to_sec());
            ep->set_oldest_item_touch(f_evict_stat_oldest_item_touch.to_sec());
            ep->set_highest_item_turn(f_evict_stat_highest_item_turn);

            return true;
        }
    }
    while(skip_empty);

    return false;
}


bool cache_impl::evict_weights()
{
    // if no evictions policy defined, then return "done" immediately
    //
    eviction_policy::pointer_t ep(f_opts.get_eviction_policy());
    if(ep == nullptr)
    {
        return true;
    }

    if(f_evict_weight_index == 0)
    {
        // reset the statistic variables
        //
        f_evict_smallest_weights.clear();

        f_evict_weight_offset = snapdev::random(0UL, f_buckets.size() - 1UL);

        // setup a default weight count
        //
        f_evict_smallest_weights_count = EVICT_SMALLEST_WEIGHT_COUNT;

        // compute the number of items we want to collect to reduce the size
        // from the current size to the reduction percent if the count trigger
        // is reached
        //
        std::size_t const current_size(size());
        std::size_t const eviction_trigger(ep->get_size_eviction_trigger_limit(f_opts.get_count_limit()));
        if(current_size >= floor(eviction_trigger))
        {
            // add a little bit so by the time we start evicting we
            // get at least enough entries
            //
            double const eviction_count(f_opts.get_count_limit() * (1.0 - ep->get_size_eviction_reduction_percent()) + 10.0);
            if(eviction_count > f_evict_smallest_weights_count)
            {
                f_evict_smallest_weights_count = eviction_count;
            }
        }
std::cerr << "--- evict_stats() count set to = "
<< f_evict_smallest_weights_count
//<< " -- max access: " << f_evict_stat_max_accesses_for_weight
//<< " -- latest access: " << f_evict_stat_latest_access_age_for_weight
<< std::endl;
    }

    // the EVICT_SWEEP_STRIDE and f_evict_weight_offset gives us a seemingly
    // random manner to go through the bucket without having to shuffle a
    // large number of indexes
    // 
    // as a result, this means we do not give one bucket a higher priority
    // over a long period of time
    //
    bool skip_empty(false);
    do
    {
        std::size_t const index((f_evict_weight_index * EVICT_SWEEP_STRIDE + f_evict_weight_offset) % f_buckets.size());
        bucket_t & bucket{ f_buckets[index] };

        {
            lock_guard lock(bucket.f_lock, false);

            skip_empty = bucket.f_entries.empty();
            for(auto & e : bucket.f_entries)
            {
                // setup this item data in the eviction policy object
                //
                ep->set_access_count(e.second.f_access_count);
                ep->set_last_updated_on(e.second.f_last_updated_on.to_sec());
                ep->set_last_touched_on(e.second.f_last_touched_on.to_sec());
                ep->set_allocated(e.second.f_allocated);
                ep->set_item_created_on(e.second.f_created_on.to_sec());
                ep->set_item_timeout(e.second.f_timeout.to_sec());
                ep->set_item_turn(e.second.f_turn);

                // now compute this item weight
                //
                double const weight(ep->compute_weight());

                // register the weight if possible
                //
                if(f_evict_smallest_weights.size() < f_evict_smallest_weights_count)
                {
                    f_evict_smallest_weights.emplace(e.first, weight, e.second.f_allocated);
                }
                else if(weight < f_evict_smallest_weights.rbegin()->f_weight)
                {
                    f_evict_smallest_weights.erase(std::prev(f_evict_smallest_weights.end()));
                    f_evict_smallest_weights.emplace(e.first, weight, e.second.f_allocated);
                }
            }
        }
std::cerr << "--- evict_stats() smallest weight: " << f_evict_smallest_weights.size() << std::endl;
//for(auto const & e : f_evict_smallest_weights)
//{
//    std::cerr << "  " << e.f_weight
//}

        ++f_evict_weight_index;
        if(f_evict_weight_index >= f_buckets.size())
        {
            f_evict_weight_index = 0;
            return true;
        }
    }
    while(skip_empty);

    return false;
}


bool cache_impl::evict_organize_by_bucket()
{
    eviction_policy::pointer_t ep(f_opts.get_eviction_policy());
    if(ep == nullptr)
    {
        return true;
    }

    std::size_t current_count(size());
    std::size_t current_allocated(allocated());

    std::size_t const count_limit(ep->get_size_eviction_reduction_limit(f_opts.get_count_limit()));
    std::size_t const capacity_limit(ep->get_size_eviction_reduction_limit(f_opts.get_capacity_limit()));

    auto need_more_evictions([count_limit, capacity_limit, &current_count, &current_allocated]()
        {
            return (count_limit > 0 && current_count > count_limit)
                || (capacity_limit > 0 && current_allocated > capacity_limit);
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

        std::size_t const bucket_idx(get_bucket_index(e.f_hash));
        f_evict_targets_by_bucket[bucket_idx].push_back(e.f_hash);

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
    eviction_policy::pointer_t ep(f_opts.get_eviction_policy());
    if(ep == nullptr)
    {
        return true;
    }

    if(!f_evict_targets_by_bucket.empty())
    {
        std::size_t const count_limit(ep->get_size_eviction_reduction_limit(f_opts.get_count_limit()));
        std::size_t const capacity_limit(ep->get_size_eviction_reduction_limit(f_opts.get_capacity_limit()));

        auto need_more_evictions([this, count_limit, capacity_limit]()
            {
                return (count_limit > 0 && this->f_current_count_usage > count_limit)
                    || (capacity_limit > 0 && this->f_current_capacity_usage > capacity_limit);
            });

        auto bucket_it(f_evict_targets_by_bucket.begin());
        std::size_t const bucket_idx(bucket_it->first);
        bucket_t & bucket{ f_buckets[bucket_idx] };

std::cerr << "--- evict from bucket: " << bucket_idx << " (" << f_evict_targets_by_bucket.size() << "/" << bucket_it->second.size() << "/" << f_current_count_usage << "/" << f_eviction_counter << ")\n";
std::cerr << "--- evict need more eviction? "
    << f_current_count_usage << " > " << count_limit
    << " and "
    << f_current_capacity_usage << " > " << capacity_limit
    << "\n";
        bool done(false);
        {
            // -------------------- LOCK --------------------
            lock_guard lock(bucket.f_lock, true);

            for(auto & h : bucket_it->second)
            {
                if(!need_more_evictions())
                {
                    done = true;
                    break;
                }

                // WARNING: at the moment, the locks are considered non-recursive
                //          because the rwlock is not--so calling erase(h) from
                //          here would end up in a deadlock
                //
                auto it(bucket.f_entries.find(h));
                if(it != bucket.f_entries.end())
                {
                    --f_current_count_usage;
                    f_current_capacity_usage -= it->second.f_allocated;
                    bucket.f_entries.erase(it);
                    ++f_eviction_counter;
                }
            }
        }

        if(done)
        {
            // this happens if we had other evictions done by the
            // user or other processes (i.e. purge_expired())
            //
            f_evict_targets_by_bucket.clear();
        }
        else
        {
            f_evict_targets_by_bucket.erase(bucket_it);
        }
std::cerr << "--- evicted from bucket: " << bucket_idx << " -- erased? (" << f_evict_targets_by_bucket.size() << "/" << f_current_count_usage << "/" << f_eviction_counter << "/" << std::boolalpha << done << ")\n";
    }

    return f_evict_targets_by_bucket.empty();
}


lock_interface::pointer_t cache_impl::get_lock(hash_t h)
{
    std::size_t const index(get_bucket_index(h));
    bucket_t & bucket{ const_cast<bucket_t &>(f_buckets[index]) };
    return bucket.f_lock;
}






} // namespace detail
} // namespace magpie
// vim: ts=4 sw=4 et
