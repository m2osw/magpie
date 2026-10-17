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
#include    <set>


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
    CATCH_START_SECTION("evict_least_recent: create many items, watch that the least recently added are removed first")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        constexpr int const     VALUES_COUNT = 2'000;
        std::random_device      rd;
        std::mt19937            g(rd());

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_cleanup_interval(1);       // 1s
            opts.set_least_recently_used(true);
            opts.set_count(VALUES_COUNT / 2);

            // construct
            //
            magpie::cache<int, int>::pointer_t cache(std::make_shared<magpie::cache<int, int>>(opts));

            // having the least recently used flag on means a cleanup thread
            // gets started by the constructor
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            int pos[VALUES_COUNT];
            for(;;)
            {
                // set once
                //
                for(int kv(1); kv <= VALUES_COUNT; ++kv)
                {
                    cache->set(kv, kv);
                    pos[kv - 1] = kv;
                }
                std::shuffle(pos, pos + VALUES_COUNT, g);

                // read many times the first 50% (indexes from pos array)
                //
                for(int count(0); count < 5; ++count)
                {
                    std::cout << "--- lock mode: " << g_lock_models[lock_model].f_name << " -- check #" << count << " ---" << std::endl;

                    // v1 should last 1h, so no issue here
                    //
                    for(int idx(0); idx < VALUES_COUNT / 2; ++idx)
                    {
                    std::optional<std::string> v(cache->get(pos[idx]));
                    CATCH_REQUIRE(v.has_value());
                    CATCH_REQUIRE(v.value() == v1);

                    v = cache->get(k2);
                    CATCH_REQUIRE(v.has_value());
                    CATCH_REQUIRE(v.value() == v2);

                    v = cache->get(k3);
                    CATCH_REQUIRE(v.has_value());
                    CATCH_REQUIRE(v.value() == v3);

                    sleep(1);
                }

                break;
            }

            // done, everything gets destroyed
            //
            cache.reset();

            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}





// vim: ts=4 sw=4 et
