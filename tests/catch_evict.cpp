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



CATCH_TEST_CASE("evict_least_recent", "[thread]")
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

                cache->verify_current_count();
                cache->verify_current_capacity();
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

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_least_frequent", "[thread]")
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

                cache->verify_current_count();
                cache->verify_current_capacity();
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

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_largest_first", "[thread]")
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

                cache->verify_current_count();
                cache->verify_current_capacity();
                sleep(1);
            }
            while(missing < VALUES_COUNT - reduced_size);

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("evict_smallest_first", "[thread]")
{
    CATCH_START_SECTION("evict_smallest_first: create many items, watch that the smallest are evicted first")
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
            ep->profile().set_allocated_weight(1.0); // a positive weight means: remove the smallest files
            ep->profile().set_age_weight(0.0);
            ep->profile().set_survival_weight(0.0);
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

                cache->verify_current_count();
                cache->verify_current_capacity();
                sleep(1);
            }
            while(missing < reduced_size);

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}





// vim: ts=4 sw=4 et
