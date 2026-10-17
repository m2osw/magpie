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



// structure cannot have defaults otherwise it makes it a complex
// type (i.e. with a constructor)
//
struct cache_key_t
{
    int     a;
    int     b;
    int     c;
};

bool operator == (cache_key_t const & lhs, cache_key_t const & rhs)
{
    return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c;
}


bool operator < (cache_key_t const & lhs, cache_key_t const & rhs)
{
    return lhs.a < rhs.a
        || (lhs.a == rhs.a && lhs.b < rhs.b)
        || (lhs.a == rhs.a && lhs.b == rhs.b && lhs.c < rhs.c);
}


constexpr magpie::lock_model_t const g_lock_models[] =
{
    magpie::lock_model_t::LOCK_MODEL_MUTEX,
    magpie::lock_model_t::LOCK_MODEL_RWLOCK,
    magpie::lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK,
};



} // no name namespace



CATCH_TEST_CASE("cache", "[cache]")
{
    CATCH_START_SECTION("cache: basic cache with keys & values as int (construct, add 3 values, get 3 values, destruct)")
    {
        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model]);

            // construct
            //
            magpie::cache<int, int> cache(opts);

            // keep k1 & k3 the same in the repeated loop
            //
            int const k1(rand());
            int const k3(rand());

            std::set<int> used_keys;
            used_keys.insert(k1);
            used_keys.insert(k3);

            std::size_t count(0);
            for(int repeat_set(0); repeat_set < 10; ++repeat_set)
            {
                CATCH_REQUIRE(count == cache.size());
                CATCH_REQUIRE(count * sizeof(int) == cache.allocated());

                // set a few values
                //
                int const v1(rand());
                cache.set(k1, v1);
                if(repeat_set == 0)
                {
                    ++count;
                }
                CATCH_REQUIRE(count == cache.size());

                int const k2(rand());
                std::pair<std::set<int>::iterator, bool> inserted(used_keys.insert(k2));
                if(inserted.second)
                {
                    ++count;
                }
                int const v2(rand());
                cache.set(k2, v2);
                CATCH_REQUIRE(count == cache.size());

                int const v3(rand());
                cache.set(k3, v3);
                if(repeat_set == 0)
                {
                    ++count;
                }
                CATCH_REQUIRE(count == cache.size());

                // verify the values
                //
                for(int repeat_get(0); repeat_get < 10; ++repeat_get)
                {
                    CATCH_REQUIRE(cache.get(k1).value() == v1);
                    CATCH_REQUIRE(cache.get(k2).value() == v2);
                    CATCH_REQUIRE(cache.get(k3).value() == v3);
                    CATCH_REQUIRE(count == cache.size());

                    // make sure a key that's not set returns a null
                    //
                    int k4(rand());
                    while(used_keys.contains(k4))
                    {
                        k4 = rand();
                    }
                    std::optional<int> v(cache.get(k4));
                    CATCH_REQUIRE_FALSE(v.has_value());
                }
            }

            // done, everything gets destroyed
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: basic cache with key defined as a simple struct & values as strings")
    {
        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model]);

            // construct
            //
            magpie::cache<cache_key_t, std::string> cache(opts);

            std::set<cache_key_t> used_keys;

            cache_key_t const k1 = {
                .a = rand(),
                .b = rand(),
                .c = rand(),
            };
            used_keys.insert(k1);

            cache_key_t const k3 = {
                .a = rand(),
                .b = rand(),
                .c = rand(),
            };
            used_keys.insert(k3);

            for(int repeat(0); repeat < 10; ++repeat)
            {
                // set a few values
                //
                std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, repeat == 0 ? 4 : 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                cache.set(k1, v1);

                cache_key_t const k2 = {
                    .a = rand(),
                    .b = rand(),
                    .c = rand(),
                };
                used_keys.insert(k2);
                std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(1, repeat == 1 ? 8 : 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                cache.set(k2, v2);

                std::string const v3(SNAP_CATCH2_NAMESPACE::random_string(1, repeat + 1, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                cache.set(k3, v3);

                // verify the values -- if it works properly, we can repeat
                //
                for(int count(0); count < 10; ++count)
                {
                    std::string g1(cache.get(k1).value());
                    SNAP_CATCH2_NAMESPACE::catch_compare_long_strings(v1, g1);
                    CATCH_REQUIRE(v1 == g1);
                    std::string g2(cache.get(k2).value());
                    SNAP_CATCH2_NAMESPACE::catch_compare_long_strings(v2, g2);
                    CATCH_REQUIRE(v2 == g2);
                    std::string g3(cache.get(k3).value());
                    SNAP_CATCH2_NAMESPACE::catch_compare_long_strings(v3, g3);
                    CATCH_REQUIRE(v3 == g3);
                }

                // make sure a key that's not set returns a null
                //
                cache_key_t k4 = {
                    .a = rand(),
                    .b = rand(),
                    .c = rand(),
                };
                while(used_keys.contains(k4))
                {
                    k4.a = rand();
                    k4.b = rand();
                    k4.c = rand();
                }
                std::optional<std::string> v(cache.get(k4));
                CATCH_REQUIRE_FALSE(v.has_value());
            }

            // done, everything gets destroyed
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: verify that an empty key is valid too")
    {
        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_allow_empty_key(true);
            opts.set_lock_model(g_lock_models[lock_model]);

            magpie::cache<std::string, std::string> cache(opts);

            // keep the same keys throughout and make sure that the
            // empty key is different from the other two
            //
            std::string const k1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
            std::string const k3(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));

            for(int count(0); count < 10; ++count)
            {
                std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                cache.set(k1, v1);

                std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                cache.set(std::string(), v2);

                std::string const v3(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                cache.set(k3, v3);

                CATCH_REQUIRE(cache.get(k1).value() == v1);
                CATCH_REQUIRE(cache.get(std::string()).value() == v2);
                CATCH_REQUIRE(cache.get(k3).value() == v3);
            }
        }
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("cache_error", "[cache][error]")
{
    CATCH_START_SECTION("cache_error: verify that an empty key if forbidden by default")
    {
        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model]);

            magpie::cache<std::string, std::string> cache(opts);

            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
            CATCH_REQUIRE_THROWS_MATCHES(
                      cache.set(std::string(), v)
                    , magpie::empty_key_not_accepted
                    , Catch::Matchers::ExceptionMessage(
                              "magpie_exception: this cache was setup to not accept the empty key."));
        }
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et
