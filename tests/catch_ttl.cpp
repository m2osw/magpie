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



CATCH_TEST_CASE("ttl", "[ttl]")
{
    CATCH_START_SECTION("ttl: test cache with items using a TTL")
    {
        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model].f_mode);
            opts.set_default_ttl(3600);     // 1h
            opts.set_cleanup_interval(3);   // 3 seconds

            // construct
            //
            magpie::cache<std::string, std::string> cache(opts);

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

                cache.set(k1, v1);      // keep default TTL (1h)
                cache.set(k2, v2, 1);
                cache.set(k3, v3, 4);

                // verify the values
                //
                for(int count(0); count < 5; ++count)
                {
                    std::cout << "--- lock mode: " << g_lock_models[lock_model].f_name << " -- check #" << count << " ---" << std::endl;

                    // v1 should last 1h, so no issue here
                    //
                    std::optional<std::string> v(cache.get(k1));
                    CATCH_REQUIRE(v.has_value());
                    CATCH_REQUIRE(v.value() == v1);

                    v = cache.get(k2);
                    if(v.has_value())
                    {
                        CATCH_REQUIRE(v.value() == v2);
                    }
                    else
                    {
                        CATCH_REQUIRE(count > 0);
                    }

                    v = cache.get(k3);
                    if(v.has_value())
                    {
                        CATCH_REQUIRE(cache.get(k3).value() == v3);
                    }
                    else
                    {
                        CATCH_REQUIRE(count > 1);
                    }

                    sleep(1);
                }

                break;
            }

            // done, everything gets destroyed
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
