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
            opts.set_default_ttl(95);       // ~1.5 minutes (1% is 0.95 which we use below)
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

                //std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                //std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                //std::string const v3(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
                std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
                std::string const v3(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
std::cerr << "+++ latest v1 = " << v1 << "\n";
std::cerr << "+++ latest v2 = " << v2 << "\n";
std::cerr << "+++ latest v3 = " << v3 << "\n";

                magpie::set_result_t status(cache.set(k1, v1));      // keep default TTL (~1.5m)
                CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                status = cache.set(k2, v2, 1);
                CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                status = cache.set(k3, v3, 4);
                CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);

                // verify the values
                //
                // within the first 3 seconds, k2's value is erased
                // within the next 3 seconds, k3's value is erased
                //
                bool two_auto_erased(false);
                bool three_auto_erased(false);
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
                        two_auto_erased = true;
                    }

                    v = cache.get(k3);
                    if(v.has_value())
                    {
                        CATCH_REQUIRE(cache.get(k3).value() == v3);
                    }
                    else
                    {
                        CATCH_REQUIRE(count > 1);
                        three_auto_erased = true;
                    }

                    sleep(1);
                }
                CATCH_REQUIRE(two_auto_erased);
                CATCH_REQUIRE(three_auto_erased);

                break;
            }

            // done, everything gets destroyed
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("ttl: test cache with one item using a TTL too small to be added")
    {
        magpie::options opts;
        opts.set_lock_model(magpie::lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK);
        opts.set_default_ttl(3600);     // 1 hour
        opts.set_cleanup_interval(60);  // 3 seconds
        magpie::cache<std::string, std::string> cache(opts);

        std::string const k1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
        std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));

        magpie::set_result_t status(cache.set(k1, v1));      // keep default TTL (~1.5m)
        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);

        status = cache.set(k1, v1, 5);   // TTL too short, do not keep
        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_REJECTED_EXPIRED == status);

        // if we prevent the set(), the value needs to be erased
        //
        std::optional<std::string> missing(cache.get(k1));
        CATCH_REQUIRE_FALSE(missing.has_value());
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("ttl: test cache with one items using a TTL of 0 or negative values (immediate eviction)")
    {
        magpie::options opts;
        opts.set_lock_model(magpie::lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK);
        opts.set_default_ttl(3600);     // 1 hour
        opts.set_cleanup_interval(60);  // 3 seconds
        magpie::cache<std::string, std::string> cache(opts);

        std::string const k1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
        std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));

        magpie::set_result_t status(cache.set(k1, v1));      // keep default TTL (~1.5m)
        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);

        status = cache.set(k1, v1, 0);  // when TTL is zero, reject immediately
        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_REJECTED_EXPIRED == status);

        // if we prevent the set(), the value needs to really not be added
        //
        std::optional<std::string> missing(cache.get(k1));
        CATCH_REQUIRE_FALSE(missing.has_value());

        status = cache.set(k1, v1);      // keep default TTL (~1.5m)
        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);

        status = cache.set(k1, v1, -100);  // when TTL is negative, reject immediately
        CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_REJECTED_EXPIRED == status);
        missing = cache.get(k1);
        CATCH_REQUIRE_FALSE(missing.has_value());
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
