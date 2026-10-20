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



CATCH_TEST_CASE("thread_one_cache", "[thread]")
{
    CATCH_START_SECTION("thread_one_cache: make sure the thread exits if not necessary anymore")
    {
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(3600);         // 1h
            opts.set_cleanup_interval(1);       // 1s

            // construct
            //
            magpie::cache<std::string, std::string>::pointer_t cache(std::make_shared<magpie::cache<std::string, std::string>>(opts));

            // having a TTL which is not magpie::NO_TTL triggers the
            // start of the thread before the constructor returns
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            for(;;)
            {
                std::string const k1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                std::string const k2(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                std::string const k3(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));

                if(k1 == k2
                || k2 == k3
                || k1 == k3)
                {
                    continue;
                }

                std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                std::string const v3(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));

                cache->set(k1, v1);
                cache->set(k2, v2);
                cache->set(k3, v3);

                // verify the values
                //
                for(int count(0); count < 5; ++count)
                {
                    std::cout << "--- lock mode: " << g_lock_models[lock_model].f_name << " -- check #" << count << " ---" << std::endl;

                    // number of threads stays at 1
                    //
                    CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

                    // v1 should last 1h, so no issue here
                    //
                    std::optional<std::string> v(cache->get(k1));
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


CATCH_TEST_CASE("thread_three_caches", "[thread]")
{
    CATCH_START_SECTION("thread_three_caches: make sure the thread exits once all caches are gone")
    {
std::cout << gettid() << ": --- MAIN THREAD ---" << std::endl;
        // we are not supposed to be running anything at this point
        //
        CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);

        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(3600);         // 1h
            opts.set_cleanup_interval(600);     // 10m

            // construct
            //
std::cout << gettid() << ": --- create 3 caches ---" << std::endl;
            magpie::cache<int, int>::pointer_t cache1(std::make_shared<magpie::cache<int, int>>(opts));
std::cout << gettid() << ": --- create 2 more caches ---" << std::endl;
            magpie::cache<int, int>::pointer_t cache2(std::make_shared<magpie::cache<int, int>>(opts));
std::cout << gettid() << ": --- create 1 more cache ---" << std::endl;
            magpie::cache<int, int>::pointer_t cache3(std::make_shared<magpie::cache<int, int>>(opts));

            // having a TTL which is not magpie::NO_TTL triggers the
            // start of the thread before the constructor returns
            // and having 1 or 3 caches still only creates a single thread
            //
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            // verify we can cache a value
            {
                int const k1(rand());
                int const k2(rand());
                int const k3(rand());

                int const v1(rand());
                int const v2(rand());
                int const v3(rand());

std::cout << gettid() << ": --- 3 caches 3 set()" << std::endl;
                cache1->set(k1, v1);
                cache2->set(k2, v2);
                cache3->set(k3, v3);

                CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

                std::optional<int> v(cache1->get(k1));
                CATCH_REQUIRE(v.has_value());
                CATCH_REQUIRE(v.value() == v1);

                v = cache2->get(k2);
                CATCH_REQUIRE(v.has_value());
                CATCH_REQUIRE(v.value() == v2);

                v = cache3->get(k3);
                CATCH_REQUIRE(v.has_value());
                CATCH_REQUIRE(v.value() == v3);
            }

            // done, everything gets destroyed
            // but the thread stays until the last cache gets deleted
            //
std::cout << gettid() << ": --- 3 caches reset()" << std::endl;
            cache1.reset();
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            cache2.reset();
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 1);

            cache3.reset();
            CATCH_REQUIRE(cppthread::thread::get_running_threads() == 0);
        }
    }
    CATCH_END_SECTION()
}


//CATCH_TEST_CASE("options_error", "[options][error]")
//{
//    CATCH_START_SECTION("options_error: verify that the TTL cannot be a negative number")
//    {
//        magpie::options opts;
//
//        for(magpie::duration_t duration(-2); duration > -100; --duration)
//        {
//            CATCH_REQUIRE_THROWS_MATCHES(
//                      opts.set_default_ttl(duration)
//                    , magpie::out_of_range
//                    , Catch::Matchers::ExceptionMessage(
//                              "out_of_range: options::set_default_ttl(): ttl must be a positive number, magpie::NO_TTL, or magpie::DEFAULT_TTL."));
//        }
//    }
//    CATCH_END_SECTION()
//}


// vim: ts=4 sw=4 et
