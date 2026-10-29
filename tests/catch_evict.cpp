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

// self
//
#include    "catch_main.h"


// magpie
//
#include    <magpie/cache.h>

#include    <magpie/exception.h>


// cppthread
//
#include    <cppthread/thread.h>


// snapdev
//
#include    <snapdev/timespec_ex.h>


// C++
//
#include    <random>


// last include
//
#include    <snapdev/poison.h>



namespace
{



struct models
{
    char const *            f_name = nullptr;
    magpie::lock_model_t    f_mode = magpie::lock_model_t::LOCK_MODEL_MUTEX;
};


constexpr models const g_lock_models[] =
{
    {
        "mutex",
        magpie::lock_model_t::LOCK_MODEL_MUTEX,
    },
    {
        "rwlock",
        magpie::lock_model_t::LOCK_MODEL_RWLOCK,
    },
    {
        "recursive rwlock",
        magpie::lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK,
    },
};



} // no name namespace



CATCH_TEST_CASE("evict_least_recent", "[evict][thread]")
{
    CATCH_START_SECTION("evict_least_recent: create many items, watch that the least recently added are removed first (LFU only)")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        constexpr std::size_t const VALUES_COUNT = 2'000;
        std::random_device          rd;
        std::mt19937                g(rd());

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(90);           // 90 seconds
            opts.set_cleanup_interval(3);       // 3s
            opts.set_count_limit(VALUES_COUNT);

            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.95);
            ep->profile().set_size_eviction_reduction_percent(0.9);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(1.0);
            ep->profile().set_allocated_weight(0.0);
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            // construct
            //
            magpie::cache<int, int>::pointer_t cache(std::make_shared<magpie::cache<int, int>>(opts));

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // fill the cache to the count limit
            //
            int pos[VALUES_COUNT];
            for(std::size_t kv(1); kv <= VALUES_COUNT; ++kv)
            {
                cache->set(kv, kv);
                pos[kv - 1] = kv;
            }
            std::shuffle(std::begin(pos), std::end(pos), g);

            // mark the lowest values as hot by increasing their counters
            // to 10 and make sure they do not disappear while doing so
            //
            for(int count(0); count < 10; ++count)
            {
                for(std::size_t idx(0); idx < VALUES_COUNT / 2; ++idx)
                {
                    std::optional<int> v(cache->get(pos[idx], true));
                    CATCH_REQUIRE(v.has_value());
                    CATCH_REQUIRE(v.value() == pos[idx]);
                }
            }

            // the eviction has not kicked in yet, we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // read many times the first 50% (indexes from pos array)
            //
            for(int count(0); count < 10; ++count)
            {
                std::cout << "--- lock mode: " << g_lock_models[lock_model].f_name << " -- check #" << count << " ---" << std::endl;
                std::cout << "--- current count: " << cache->size() << std::endl;

                // v1 should last 1h, so no issue here
                //
                for(std::size_t idx(0); idx < VALUES_COUNT / 2; ++idx)
                {
                    for(int repeat(0); repeat < 3; ++repeat)
                    {
                        std::optional<int> v(cache->get(pos[idx], true));
                        CATCH_REQUIRE(v.has_value());
                        CATCH_REQUIRE(v.value() == pos[idx]);
                    }
                }

                // these require us to pause the background thread, which isn't done yet
                //
                //cache->verify_current_count();
                //cache->verify_current_capacity();
                sleep(1);
            }

            std::size_t missing(0);
            for(std::size_t idx(0); idx < std::size(pos); ++idx)
            {
                std::optional<int> v(cache->get(pos[idx], true));
                if(!v.has_value())
                {
                    ++missing;
                }
                else
                {
                    CATCH_REQUIRE(v.value() == pos[idx]);
                }
            }
            CATCH_REQUIRE(missing > VALUES_COUNT * (1.0 - 0.92)); // it should be closer to 0.9 than 0.92, but this is an approximation for the test

            // the eviction is done now so we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_least_frequent", "[evict][thread]")
{
    CATCH_START_SECTION("evict_least_frequent: create many items, watch that the least recently added are removed first (LFU only)")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        constexpr std::size_t const VALUES_COUNT = 2'000;
        std::random_device          rd;
        std::mt19937                g(rd());

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(90);           // 90 seconds
            opts.set_cleanup_interval(3);       // 3s
            opts.set_count_limit(VALUES_COUNT);

            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.95);
            ep->profile().set_size_eviction_reduction_percent(0.9);
            ep->profile().set_access_count_weight(1.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(0.0);
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            // construct
            //
            magpie::cache<int, int>::pointer_t cache(std::make_shared<magpie::cache<int, int>>(opts));

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // fill the cache to the count limit
            //
            int pos[VALUES_COUNT];
            for(std::size_t kv(1); kv <= VALUES_COUNT; ++kv)
            {
                cache->set(kv, kv);
                pos[kv - 1] = kv;
            }
            std::shuffle(std::begin(pos), std::end(pos), g);

            // the eviction has not kicked in yet, we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // mark the lowest values as hot by increasing their counters
            // to 10 and make sure they do not disappear while doing so
            //
            for(int count(0); count < 10; ++count)
            {
                for(std::size_t idx(0); idx < VALUES_COUNT / 2; ++idx)
                {
                    std::optional<int> v(cache->get(pos[idx], true));
                    CATCH_REQUIRE(v.has_value());
                    CATCH_REQUIRE(v.value() == pos[idx]);
                }
            }

            // read many times the first 50% (indexes from pos array)
            //
            for(int count(0); count < 10; ++count)
            {
                std::cout << "--- lock mode: " << g_lock_models[lock_model].f_name << " -- check #" << count << " ---" << std::endl;
                std::cout << "--- current count: " << cache->size() << std::endl;

                // v1 should last 1h, so no issue here
                //
                for(std::size_t idx(0); idx < VALUES_COUNT / 2; ++idx)
                {
                    for(int repeat(0); repeat < 3; ++repeat)
                    {
                        std::optional<int> v(cache->get(pos[idx], true));
                        CATCH_REQUIRE(v.has_value());
                        CATCH_REQUIRE(v.value() == pos[idx]);
                    }
                }

                // these require us to pause the background thread, which isn't done yet
                //
                //cache->verify_current_count();
                //cache->verify_current_capacity();
                sleep(1);
            }

            std::size_t missing(0);
            for(std::size_t idx(0); idx < std::size(pos); ++idx)
            {
                std::optional<int> v(cache->get(pos[idx], true));
                if(!v.has_value())
                {
                    ++missing;
                }
                else
                {
                    CATCH_REQUIRE(v.value() == pos[idx]);
                }
            }
            CATCH_REQUIRE(missing > VALUES_COUNT * (1.0 - 0.92)); // it should be closer to 0.9 than 0.92, but this is an approximation for the test

            // the eviction is done now so we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_largest_first", "[evict][thread]")
{
    CATCH_START_SECTION("evict_largest_first: create many items, watch that the largest are evicted first")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        constexpr std::size_t const VALUES_COUNT = 2'000;
        std::random_device          rd;
        std::mt19937                g(rd());

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(86400);        // 1 day
            opts.set_cleanup_interval(3);       // 3s
            opts.set_count_limit(VALUES_COUNT);

            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.95);
            ep->profile().set_size_eviction_reduction_percent(0.9);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(-1.0); // to remove the largest files, use a negative weight
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            // construct
            //
            typedef std::vector<std::uint8_t> buffer_t;
            magpie::cache<int, buffer_t>::pointer_t cache(std::make_shared<magpie::cache<int, buffer_t>>(opts));

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // create the buffers
            //
            std::vector<buffer_t> buffers(VALUES_COUNT);
            std::vector<std::size_t> sizes(VALUES_COUNT);
            for(std::size_t i(0); i < VALUES_COUNT; ++i)
            {
                std::size_t const size(snapdev::random(1, 2'000)); // x VALUES_COUNT, a max. of about 4Mb
                sizes[i] = size;
                buffers[i].resize(size);
                for(std::size_t j(0); j < size; ++j)
                {
                    buffers[i][j] = snapdev::random(0, 255);
                }
            }
            std::sort(sizes.begin(), sizes.end());

            std::size_t const reduced_size(static_cast<std::size_t>(VALUES_COUNT * 0.9));
            CATCH_REQUIRE(reduced_size < sizes.size());
            std::size_t const size_limit(sizes[reduced_size]);
            std::vector<bool> kept(VALUES_COUNT);
            for(std::size_t i(0); i < VALUES_COUNT; ++i)
            {
                if(buffers[i].size() < size_limit)
                {
                    kept[i] = true;
                }
            }

            // fill the cache
            //
            for(std::size_t k(0); k < VALUES_COUNT; ++k)
            {
                cache->set(k, buffers[k]);
            }

            // the eviction has not kicked in yet, we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // now the eviction is going to kick in within about 3 seconds
            // and remove the 200 largest entries; the kept array tells us
            // which buffers are expected to not go away
            //
            std::size_t missing(0);
            do
            {
                missing = 0;
                for(std::size_t k(0); k < VALUES_COUNT; ++k)
                {
                    std::optional<buffer_t> v(cache->get(k, true)); // the TTL update should have no side effects
                    if(kept[k])
                    {
                        CATCH_REQUIRE(v.has_value());
                        CATCH_REQUIRE(v.value() == buffers[k]);
                    }
                    else if(v.has_value())
                    {
                        // not yet removed
                        //
                        CATCH_REQUIRE(v.value() == buffers[k]);
                    }
                    else
                    {
                        ++missing;
                    }
                }

                // these require us to pause the background thread, which isn't done yet
                //
                //cache->verify_current_count();
                //cache->verify_current_capacity();
                sleep(1);
            }
            while(missing < VALUES_COUNT - reduced_size);

            // the eviction is done now so we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_smallest_first", "[evict][thread]")
{
    CATCH_START_SECTION("evict_smallest_first: create many items, watch that the smallest are evicted first")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        constexpr std::size_t const VALUES_COUNT = 2'000;

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(86400);        // 1 day
            opts.set_cleanup_interval(3);       // 3s
            opts.set_count_limit(VALUES_COUNT);

            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.95);
            ep->profile().set_size_eviction_reduction_percent(0.9);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(1.0); // a positive weight means: remove the smallest files
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            // construct
            //
            typedef std::vector<std::uint8_t> buffer_t;
            magpie::cache<int, buffer_t>::pointer_t cache(std::make_shared<magpie::cache<int, buffer_t>>(opts));

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // create the buffers
            //
            std::vector<buffer_t> buffers(VALUES_COUNT);
            std::vector<std::size_t> sizes(VALUES_COUNT);
            for(std::size_t i(0); i < VALUES_COUNT; ++i)
            {
                std::size_t const size(snapdev::random(1, 2'000)); // x VALUES_COUNT, a max. of about 4Mb
                sizes[i] = size;
                buffers[i].resize(size);
                for(std::size_t j(0); j < size; ++j)
                {
                    buffers[i][j] = snapdev::random(0, 255);
                }
            }
            std::sort(sizes.begin(), sizes.end());

            std::size_t const reduced_size(VALUES_COUNT - static_cast<std::size_t>(VALUES_COUNT * 0.9));
            CATCH_REQUIRE(reduced_size < sizes.size());
            std::size_t const size_limit(sizes[reduced_size]);
            std::vector<bool> kept(VALUES_COUNT);
            for(std::size_t i(0); i < VALUES_COUNT; ++i)
            {
                if(buffers[i].size() > size_limit)
                {
                    kept[i] = true;
                }
            }

            // fill the cache
            //
            for(std::size_t k(0); k < VALUES_COUNT; ++k)
            {
                cache->set(k, buffers[k]);
            }

            // the eviction has not kicked in yet, we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // now the eviction is going to kick in within about 3 seconds
            // and remove the 200 largest entries; the kept array tells us
            // which buffers are expected to not go away
            //
            std::size_t missing(0);
            do
            {
                missing = 0;
                for(std::size_t k(0); k < VALUES_COUNT; ++k)
                {
                    std::optional<buffer_t> v(cache->get(k, true)); // the TTL update should have no side effects
                    if(kept[k])
                    {
                        CATCH_REQUIRE(v.has_value());
                        CATCH_REQUIRE(v.value() == buffers[k]);
                    }
                    else if(v.has_value())
                    {
                        // not yet removed
                        //
                        CATCH_REQUIRE(v.value() == buffers[k]);
                    }
                    else
                    {
                        ++missing;
                    }
                }

                // these require us to pause the background thread, which isn't done yet
                //
                //cache->verify_current_count();
                //cache->verify_current_capacity();
                sleep(1);
            }
            while(missing < reduced_size);

            // the eviction is done now so we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_fifo", "[evict][thread]")
{
    CATCH_START_SECTION("evict_fifo: create many items, watch that the smallest are evicted first")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(86400);        // 1 day
            opts.set_cleanup_interval(300);     // 5m
            opts.set_capacity_limit(10'000);    // ~10kb

            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.9);
            ep->profile().set_size_eviction_reduction_percent(0.5);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(0.0);
            ep->profile().set_age_weight(1.0); // by itself it represents the FIFO mode
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            // construct
            //
            typedef std::vector<std::uint8_t> buffer_t;
            magpie::cache<int, buffer_t>::pointer_t cache(std::make_shared<magpie::cache<int, buffer_t>>(opts));

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // create & save the buffers in the cache
            // capacity will be overflown at some point, we stop the loop then
            // that even should immediately kickstart the eviction process
            //
            std::vector<buffer_t> buffers;
            for(int k(0);; ++k)
            {
                std::size_t size(snapdev::random(1, 200));
                buffer_t b(size);
                for(std::size_t j(0); j < size; ++j)
                {
                    b[j] = snapdev::random(0, 255);
                }
                magpie::set_result_t const r(cache->set(k, b));
std::cout << "--- loop #" << k << " -- current capacity: " << cache->allocated() << " -> " << static_cast<int>(r) << std::endl;
                if(r == magpie::set_result_t::SET_RESULT_REJECTED_CAPACITY)
                {
                    break;
                }
                buffers.push_back(b);
                if(cache->allocated() < opts.get_capacity_limit() * 0.6)
                {
                    sleep(1);  // give the first 60% of the necessary buffers a different f_created_on value
                }

                std::optional<buffer_t> v(cache->get(0));
                if(!v.has_value())
                {
                    // item 0 was removed, we missed the SET_RESULT_REJECTED_CAPACITY
                    // during our sleep it looks like
                    //
                    break;
                }
            }

            // added to the cache
            //
            //std::vector<bool> added;

            //std::size_t const reduced_size(VALUES_COUNT - static_cast<std::size_t>(VALUES_COUNT * 0.9));
            //CATCH_REQUIRE(reduced_size < sizes.size());
            //std::size_t const size_limit(sizes[reduced_size]);
            //for(std::size_t i(0); i < VALUES_COUNT; ++i)
            //{
            //    if(buffers[i].size() > size_limit)
            //    {
            //        kept[i] = true;
            //    }
            //}

            // fill the cache
            //
            //for(std::size_t k(0); k < buffers.size(); ++k)
            //{
            //}

            // the eviction has not kicked in yet, we can check these here
            //
            //cache->verify_current_count();
            //cache->verify_current_capacity();

            // the eviction should have kicked in when we tried to add the
            // last buffer because it was too large...
            //
            std::size_t missing(0);
            do
            {
                missing = 0;
                for(std::size_t k(0); k < buffers.size(); ++k)
                {
                    std::optional<buffer_t> v(cache->get(k, rand() % 2 == 0)); // the TTL update should have no side effects
                    if(v.has_value())
                    {
                        // not yet removed
                        //
                        CATCH_REQUIRE(v.value() == buffers[k]);
                    }
                    else
                    {
                        // in FIFO mode, only the lower half should disappear
                        //
                        CATCH_REQUIRE(k <= 2 * buffers.size() / 3);
                        ++missing;
                    }
                }
std::cout << "--- loop -- missing: " << missing << " -- current capacity: " << cache->allocated() << std::endl;

                // these require us to pause the background thread, which isn't done yet
                //
                //cache->verify_current_count();
                //cache->verify_current_capacity();
                sleep(1);
            }
            while(missing < buffers.size() / 3); // it should go around buffers.size() / 2, but proof of a few on the correct side is sufficient

            // the eviction is done now so we can check these here
            //
            cache->verify_current_count();
            cache->verify_current_capacity();

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_survival", "[evict][thread]")
{
    CATCH_START_SECTION("evict_survival: fill up the cache so TTL is used by the eviction process instead of the purge process")
    {
        constexpr std::size_t const VALUES_COUNT = 30;

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(86400);    // 1 day
            opts.set_cleanup_interval(3);   // 3 seconds
            opts.set_count_limit(VALUES_COUNT);

            // use a random policy but if an item has many turns, increase
            // its likelihood to be evicted
            //
            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.9);
            ep->profile().set_size_eviction_reduction_percent(0.5);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(0.0);
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(1.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            // construct
            //
            magpie::cache<int, int> cache(opts);

            for(std::size_t w(0), r(0); r < VALUES_COUNT * 2; ++w)
            {
                magpie::set_result_t status(cache.set(w, w));
                if(status != magpie::set_result_t::SET_RESULT_ADDED
                && status != magpie::set_result_t::SET_RESULT_REJECTED_COUNT)
                {
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                }

                // when the delete occurs, it's not necessarily in order
                // because it's done on a per bucket basis; but if one
                // item was evicted, the previous ones cannot reappear
                //
                for(std::size_t evicted(0);; ++evicted)
                {
                    magpie::cache<int, int>::optional_t v(cache.get(evicted));
                    if(v.has_value())
                    {
                        CATCH_REQUIRE(r == evicted);
                        CATCH_REQUIRE(r == static_cast<std::size_t>(v.value()));
                        break;
                    }
                    if(evicted == r)
                    {
                        ++r;
                    }
                }

                sleep(1);
            }


            // done, everything gets destroyed
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_turn", "[evict][thread]")
{
    CATCH_START_SECTION("evict_turn: test 100 items, refill a few times so we can see the eviction turn growing")
    {
        constexpr std::size_t const VALUES_COUNT = 100;
        std::random_device          rd;
        std::mt19937                g(rd());

        int indexes[VALUES_COUNT];
        std::iota(std::begin(indexes), std::end(indexes), 0);

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_cleanup_interval(3);   // 3 seconds
            opts.set_count_limit(VALUES_COUNT);

            // use a random policy but if an item has many turns, increase
            // its likelihood to be evicted
            //
            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.9);
            ep->profile().set_size_eviction_reduction_percent(0.5);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(0.0);
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(1.0);
            ep->profile().set_random_weight(0.2);
            opts.set_eviction_policy(ep);

            // construct
            //
            magpie::cache<int, int> cache(opts);

            std::shuffle(std::begin(indexes), std::end(indexes), g);
            for(std::size_t idx(0); idx < std::size(indexes); ++idx)
            {
                magpie::set_result_t status(cache.set(indexes[idx], rand()));
                CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
            }

            std::uint32_t max_turn(0);
            for(;;)
            {
                std::shuffle(std::begin(indexes), std::end(indexes), g);
                for(std::size_t idx(0); idx < std::size(indexes); ++idx)
                {
                    magpie::set_result_t status(cache.set(indexes[idx], rand()));
                    if(magpie::set_result_t::SET_RESULT_ADDED != status
                    && magpie::set_result_t::SET_RESULT_UPDATED != status)
                    {
                        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                    }

                    // if the eviction works as expected, then the f_turn of
                    // an item can never go up higher than 2 or 3 before it
                    // gets evicted (so far the highest I've seen is 2)
                    //
                    magpie::metadata_t const info(cache.get_metadata(indexes[idx]));
                    CATCH_REQUIRE(info.f_turn < 4);

                    if(info.f_turn > max_turn)
                    {
                        max_turn = info.f_turn;
                    }
                }

                // run 10 cycles to really prove that the eviction works
                //
                if(ep->cycle().get_cycle_turn() >= 10)
                {
                    break;
                }

                sleep(1);
            }
            std::cout << "--- max turn in item = " << max_turn << std::endl;


            // done, everything gets destroyed
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_wakeup", "[evict][thread]")
{
    CATCH_START_SECTION("evict_wakeup: create many items, make sure the first deletions happen quickly")
    {
        constexpr std::size_t const     VALUE_COUNT = 100;

        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(86400);        // 1 day
            opts.set_cleanup_interval(300);     // 5m
            opts.set_count_limit(VALUE_COUNT);

            magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
            ep->profile().set_size_eviction_trigger_percent(0.9);
            ep->profile().set_size_eviction_reduction_percent(0.5);
            ep->profile().set_access_count_weight(0.0);
            ep->profile().set_recency_weight(0.0);
            ep->profile().set_allocated_weight(0.0);
            ep->profile().set_age_weight(1.0); // by itself it represents the FIFO mode
            ep->profile().set_survival_weight(0.0);
            ep->profile().set_turn_weight(0.0);
            ep->profile().set_random_weight(0.0);
            opts.set_eviction_policy(ep);

            magpie::cache<int, int> cache(opts);

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // create & save the buffers in the cache
            // capacity will be overflown at some point, we stop the loop then
            // that even should immediately kickstart the eviction process
            //
            std::size_t k(0);
            for(;; ++k)
            {
                magpie::set_result_t const r(cache.set(k, rand()));
                if(r != magpie::set_result_t::SET_RESULT_ADDED)
                {
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_REJECTED_COUNT == r);
                    break;
                }
            }

            snapdev::timespec_ex const quick_sleep(0, 1'000);
            snapdev::timespec_ex const start(snapdev::now());
            for(std::size_t l(0);; ++l)
            {
                std::optional<int> v(cache.get(l % k));
                if(!v.has_value())
                {
                    // item 0 was removed, we missed the SET_RESULT_REJECTED_CAPACITY
                    // during our sleep it looks like
                    //
                    break;
                }

                // if the test is failing, return anyway
                //
                snapdev::timespec_ex const check_end(snapdev::now());
                snapdev::timespec_ex const check_duration(check_end - start);
                if(check_duration.tv_sec > 10)
                {
                    break;
                }

                // without this, the get() are really fast possibly
                // preventing the eviction from running
                //
                nanosleep(&quick_sleep, nullptr);
            }
            snapdev::timespec_ex const end(snapdev::now());
            snapdev::timespec_ex const duration(end - start);
            CATCH_REQUIRE(duration.tv_sec < 5);
        }
    }
    CATCH_END_SECTION()
}






// vim: ts=4 sw=4 et
