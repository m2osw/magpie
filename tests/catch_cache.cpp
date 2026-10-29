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
                magpie::set_result_t status(cache.set(k1, v1));
                if(repeat_set == 0)
                {
                    ++count;
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                }
                else
                {
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_UPDATED == status);
                }
                CATCH_REQUIRE(count == cache.size());

                int const k2(rand());
                std::pair<std::set<int>::iterator, bool> inserted(used_keys.insert(k2));
                if(inserted.second)
                {
                    ++count;
                }
                int const v2(rand());
                status = cache.set(k2, v2);
                if(inserted.second)
                {
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                }
                else
                {
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_UPDATED == status);
                }
                CATCH_REQUIRE(count == cache.size());

                int const v3(rand());
                status = cache.set(k3, v3);
                if(repeat_set == 0)
                {
                    ++count;
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_ADDED == status);
                }
                else
                {
                    CATCH_REQUIRE(magpie::set_result_t::SET_RESULT_UPDATED == status);
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

            // if the value does not exist, we get an empty T
            //
            std::string const empty_default(const_cast<magpie::cache<cache_key_t, std::string> const &>(cache)[k1]);
            CATCH_REQUIRE(empty_default.empty());

            auto const by_ref(cache[k3]);
            std::string const also_empty(by_ref);
            CATCH_REQUIRE(also_empty.empty());

            for(int repeat(0); repeat < 10; ++repeat)
            {
                // set a few values
                //
                std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, repeat == 0 ? 4 : 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                if((rand() & 1) != 0)
                {
                    cache.set(k1, v1);
                }
                else
                {
                    cache[k1] = v1;
                }

                cache_key_t const k2 = {
                    .a = rand(),
                    .b = rand(),
                    .c = rand(),
                };
                used_keys.insert(k2);
                std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(1, repeat == 1 ? 8 : 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                if((rand() & 1) != 0)
                {
                    cache.set(k2, v2);
                }
                else
                {
                    cache[k2] = v2;
                }

                std::string const v3(SNAP_CATCH2_NAMESPACE::random_string(1, repeat + 1, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
                if((rand() & 1) != 0)
                {
                    cache.set(k3, v3);
                }
                else
                {
                    cache[k3] = v3;
                }

                // verify the values -- if it works properly, we can repeat
                //
                for(int count(0); count < 10; ++count)
                {
                    std::string g1;
                    switch(rand() % 3)
                    {
                    case  0:
                        g1 = cache.get(k1).value();
                        break;

                    case 1:
                        g1 = cache[k1];
                        break;

                    case 2:
                        g1 = *cache[k1];
                        break;

                    }
                    SNAP_CATCH2_NAMESPACE::catch_compare_long_strings(v1, g1);
                    CATCH_REQUIRE(v1 == g1);

                    std::string g2;
                    switch(rand() % 3)
                    {
                    case 0:
                        g2 = cache.get(k2).value();
                        break;

                    case 1:
                        {
                            auto ref = cache[k2];
                            auto ref2 = ref;
                            ref2 = ref2;   // make sure copy to itself doesn't blowup anything
                            g2 = ref2;
                        }
                        break;

                    case 2:
                        g2 = *cache[k2];
                        break;

                    }
                    SNAP_CATCH2_NAMESPACE::catch_compare_long_strings(v2, g2);
                    CATCH_REQUIRE(v2 == g2);

                    std::string g3;
                    switch(rand() % 3)
                    {
                    case 0:
                        g3 = cache.get(k3).value();
                        break;

                    case 1:
                        {
                            auto ref(cache[k3]);
                            auto const ref2(ref);
                            g3 = ref2;
                        }
                        break;

                    case 2:
                        g3 = *cache[k3];
                        break;

                    }
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

    CATCH_START_SECTION("cache: object on its own is too large")
    {
        magpie::options opts;
        opts.set_maximum_object_size(100);

        magpie::cache<std::string, std::string> cache(opts);

        std::string const k(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));

        // first value is valid, it sticks
        //
        std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        magpie::set_result_t status(cache.set(k, v1));
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
        CATCH_REQUIRE(cache.get(k).has_value());
        CATCH_REQUIRE(cache.get(k).value() == v1);

        // second value is too large, it doesn't get added and removes the old value
        //
        std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(101, 200, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        status = cache.set(k, v2);
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_REJECTED_TOO_LARGE);
        CATCH_REQUIRE_FALSE(cache.get(k).has_value());
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: object was banned")
    {
        magpie::options opts;

        magpie::cache<std::string, std::string> cache(opts);

        std::string const banned(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        cache.ban(banned);

        CATCH_REQUIRE(cache.size() == 0);

        // first value add a non-banned item
        //
        std::string const k(SNAP_CATCH2_NAMESPACE::random_string(33, 64, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 64, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        magpie::set_result_t status(cache.set(k, v));
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
        CATCH_REQUIRE(cache.get(k).has_value());
        CATCH_REQUIRE(cache.get(k).value() == v);
        CATCH_REQUIRE(cache.size() == 1);

        // then try to add that value with the banned key
        //
        status = cache.set(banned, v);
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_REJECTED_BANNED);
        CATCH_REQUIRE_FALSE(cache.get(banned).has_value());
        CATCH_REQUIRE(cache.get(k).has_value()); // and k is still around
        CATCH_REQUIRE(cache.get(k).value() == v);
        CATCH_REQUIRE(cache.size() == 1);

        // but if we unban, then we can add it
        //
        cache.unban(banned);
        status = cache.set(banned, v);
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
        CATCH_REQUIRE(cache.get(banned).has_value());
        CATCH_REQUIRE(cache.get(banned).value() == v);
        CATCH_REQUIRE(cache.get(k).has_value());
        CATCH_REQUIRE(cache.get(k).value() == v);
        CATCH_REQUIRE(cache.size() == 2);

        // now if we re-ban it, the value is gone
        //
        cache.ban(banned);
        CATCH_REQUIRE_FALSE(cache.get(banned).has_value());
        CATCH_REQUIRE(cache.get(k).has_value());
        CATCH_REQUIRE(cache.get(k).value() == v);
        CATCH_REQUIRE(cache.size() == 1);
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: too many objects in the cache, more are not accepted")
    {
        magpie::options opts;
        opts.set_count_limit(10);

        magpie::cache<int, std::string> cache(opts);

        for(int k(1); k <= 10; ++k)
        {
            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
            magpie::set_result_t status(cache.set(k, v));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
            CATCH_REQUIRE(cache.get(k).has_value());
            CATCH_REQUIRE(cache.get(k).value() == v);
            CATCH_REQUIRE(cache.size() == static_cast<std::size_t>(k));
        }
        for(int k(101); k <= 110; ++k)
        {
            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
            magpie::set_result_t status(cache.set(k, v));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_REJECTED_COUNT);
            CATCH_REQUIRE_FALSE(cache.get(k).has_value());
            CATCH_REQUIRE(cache.size() == 10);
        }

        // however, updating existing items work
        //
        for(int k(1); k <= 10; ++k)
        {
            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
            magpie::set_result_t status(cache.set(k, v));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_UPDATED);
            CATCH_REQUIRE(cache.get(k).has_value());
            CATCH_REQUIRE(cache.get(k).value() == v);
            CATCH_REQUIRE(cache.size() == 10);
        }

        // however, if we delete one of the original, we can add more
        {
            cache.erase(3);
            CATCH_REQUIRE(cache.size() == 9);
            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
            magpie::set_result_t status(cache.set(101, v));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
            CATCH_REQUIRE(cache.get(101).has_value());
            CATCH_REQUIRE(cache.get(101).value() == v);
            CATCH_REQUIRE(cache.size() == 10);
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: object with no value")
    {
        magpie::options opts;
        opts.set_count_limit(25);

        magpie::empty_t const v;

        magpie::cache<int, magpie::empty_t> cache(opts);

        // create a few items
        //
        for(int k(1); k <= 10; ++k)
        {
            magpie::set_result_t status(cache.set(k, v));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
            CATCH_REQUIRE(cache.get(k).has_value());
            //CATCH_REQUIRE(cache.get(k).value() == v);
            CATCH_REQUIRE(cache.size() == static_cast<std::size_t>(k));
        }

        // items not created do not exist
        //
        for(int k(101); k <= 110; ++k)
        {
            CATCH_REQUIRE_FALSE(cache.get(k).has_value());
            CATCH_REQUIRE(cache.size() == 10UL);
        }

        // updating items
        //
        for(int k(1); k <= 10; ++k)
        {
            magpie::set_result_t status(cache.set(k, v));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_UPDATED);
            CATCH_REQUIRE(cache.get(k).has_value());
            //CATCH_REQUIRE(cache.get(k).value() == v);
            CATCH_REQUIRE(cache.size() == 10UL);
        }

        // we can erase one of the items
        {
            cache.erase(3);
            CATCH_REQUIRE(cache.size() == 9UL);
            CATCH_REQUIRE_FALSE(cache.get(3).has_value());
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: objects using too much memory, reject additional entries")
    {
        magpie::options opts;
        opts.set_capacity_limit(10'000);

        magpie::cache<int, std::string> cache(opts);

        int k(1);
        for(;; ++k)
        {
            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(30, 200, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
            magpie::set_result_t status(cache.set(k, v));
            if(status == magpie::set_result_t::SET_RESULT_REJECTED_CAPACITY)
            {
                CATCH_REQUIRE_FALSE(cache.get(k).has_value());
                break;
            }

            // capacity not reached yet, add the item
            //
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
            CATCH_REQUIRE(cache.get(k).has_value());
            CATCH_REQUIRE(cache.get(k).value() == v);
            CATCH_REQUIRE(cache.size() == static_cast<std::size_t>(k));
        }

        // first try to shrink a few things, that should always work
        //
        for(int i(1); i < k && i <= 15; ++i)
        {
            std::optional<std::string> v(cache.get(i));
            CATCH_REQUIRE(v.has_value());
            std::string s(v.value());
            s.resize(s.size() / 2);
            magpie::set_result_t status(cache.set(i, s));
            CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_UPDATED);
        }

        // append to strings
        //
        // IMPORTANT: even though it looks like we're adding very
        //            short strings; the std::string sizing makes use
        //            of a capacity feature and thus even with a +1 char
        //            we may end up adding 200 bytes at once (doubling
        //            the capacity)
        //
        for(int i(1);; ++i)
        {
            if(i >= k)
            {
                i = 1;
            }
            std::optional<std::string> v(cache.get(i));
            CATCH_REQUIRE(v.has_value());
            std::string s(v.value());
            std::string const add(SNAP_CATCH2_NAMESPACE::random_string(1, 5, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
            s += add;
            magpie::set_result_t status(cache.set(i, s));
            if(status == magpie::set_result_t::SET_RESULT_REJECTED_CAPACITY)
            {
                // in this second case, since we were updating that item,
                // the result is that it just got kicked out of the cache
                // entirely; otherwise it would return an invalid item if
                // retrieved from the cache
                //
                CATCH_REQUIRE_FALSE(cache.contains(i));
                break;
            }
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: check metadata without maximum lifetime")
    {
        magpie::options opts;
        magpie::cache<int, std::string> cache(opts);

        // before we add the key, there is no metadata
        //
        magpie::metadata_t info(cache.get_metadata(33));
        CATCH_REQUIRE_FALSE(info.is_defined());

        std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(30, 200, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        magpie::set_result_t status(cache.set(33, v1));
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);

        // now it's defined
        //
        info = cache.get_metadata(33);
        CATCH_REQUIRE(info.is_defined());

        CATCH_REQUIRE(info.f_size == sizeof(std::string));
        CATCH_REQUIRE(snapdev::is_between(info.f_allocated, v1.capacity(), v1.capacity() + 100));
        CATCH_REQUIRE(info.f_update_count == 1);
        CATCH_REQUIRE(info.f_touch_count == 0);
        CATCH_REQUIRE(snapdev::is_between(info.f_created_on, time(nullptr) - 5, time(nullptr) + 5));
        CATCH_REQUIRE(snapdev::is_between(info.f_last_updated_on, time(nullptr) - 5, time(nullptr) + 5));
        CATCH_REQUIRE(info.f_last_touched_on == time_t());
        CATCH_REQUIRE(info.f_timeout == time_t());
        CATCH_REQUIRE(info.f_stale_at == info.f_timeout);
        CATCH_REQUIRE(info.f_ttl == magpie::NO_TTL);
        CATCH_REQUIRE(snapdev::quiet_floating_point_equal(info.f_cost, magpie::NO_COST));

        std::optional<std::string> s(cache.get(33));
        CATCH_REQUIRE(s.has_value());
        CATCH_REQUIRE(s.value() == v1);

        info = cache.get_metadata(33);
        CATCH_REQUIRE(info.is_defined());
        CATCH_REQUIRE(info.f_update_count == 1);
        CATCH_REQUIRE(info.f_touch_count == 1);
        CATCH_REQUIRE(snapdev::is_between(info.f_last_touched_on, time(nullptr) - 5, time(nullptr) + 5));

        std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(30, 200, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        status = cache.set(33, v2, 60, 0.67f);
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_UPDATED);

        info = cache.get_metadata(33);
        CATCH_REQUIRE(info.is_defined());
        CATCH_REQUIRE(info.f_update_count == 2);
        CATCH_REQUIRE(info.f_touch_count == 1);
        CATCH_REQUIRE(snapdev::is_between(info.f_last_updated_on, time(nullptr) - 5, time(nullptr) + 5));
        CATCH_REQUIRE(snapdev::is_between(info.f_timeout, time(nullptr) + 55, time(nullptr) + 65));
        CATCH_REQUIRE(info.f_stale_at == info.f_timeout);
        CATCH_REQUIRE(info.f_ttl == 60);
        CATCH_REQUIRE(snapdev::quiet_floating_point_equal(info.f_cost, 0.67f));
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: check metadata with a 1 day maximum lifetime")
    {
        magpie::options opts;
        opts.set_maximum_lifetime(86400); // 1 day
        magpie::cache<int, std::string> cache(opts);

        // before we add the key, there is no metadata
        //
        magpie::metadata_t info(cache.get_metadata(33));
        CATCH_REQUIRE_FALSE(info.is_defined());

        std::string const v1(SNAP_CATCH2_NAMESPACE::random_string(30, 200, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        magpie::set_result_t status(cache.set(33, v1));
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);

        // now it's defined
        //
        info = cache.get_metadata(33);
        CATCH_REQUIRE(info.is_defined());

        CATCH_REQUIRE(info.f_size == sizeof(std::string));
        CATCH_REQUIRE(snapdev::is_between(info.f_allocated, v1.capacity(), v1.capacity() + 100));
        CATCH_REQUIRE(info.f_update_count == 1);
        CATCH_REQUIRE(info.f_touch_count == 0);
        CATCH_REQUIRE(snapdev::is_between(info.f_created_on, time(nullptr) - 5, time(nullptr) + 5));
        CATCH_REQUIRE(snapdev::is_between(info.f_last_updated_on, time(nullptr) - 5, time(nullptr) + 5));
        CATCH_REQUIRE(info.f_last_touched_on == time_t());
        CATCH_REQUIRE(snapdev::is_between(info.f_stale_at, time(nullptr) + 86400 - 5, time(nullptr) + 86400 + 5));
        CATCH_REQUIRE(info.f_timeout == time_t());
        CATCH_REQUIRE(info.f_ttl == magpie::NO_TTL);
        CATCH_REQUIRE(snapdev::quiet_floating_point_equal(info.f_cost, magpie::NO_COST));

        std::optional<std::string> s(cache.get(33));
        CATCH_REQUIRE(s.has_value());
        CATCH_REQUIRE(s.value() == v1);

        info = cache.get_metadata(33);
        CATCH_REQUIRE(info.is_defined());
        CATCH_REQUIRE(info.f_update_count == 1);
        CATCH_REQUIRE(info.f_touch_count == 1);
        CATCH_REQUIRE(snapdev::is_between(info.f_last_touched_on, time(nullptr) - 5, time(nullptr) + 5));

        std::string const v2(SNAP_CATCH2_NAMESPACE::random_string(30, 200, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_ASCII));
        status = cache.set(33, v2, 60, 0.67f);
        CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_UPDATED);

        info = cache.get_metadata(33);
        CATCH_REQUIRE(info.is_defined());
        CATCH_REQUIRE(info.f_update_count == 2);
        CATCH_REQUIRE(info.f_touch_count == 1);
        CATCH_REQUIRE(snapdev::is_between(info.f_last_updated_on, time(nullptr) - 5, time(nullptr) + 5));
        CATCH_REQUIRE(snapdev::is_between(info.f_timeout, time(nullptr) + 55, time(nullptr) + 65));
        CATCH_REQUIRE(info.f_stale_at == info.f_timeout); // TTL is much shorter than maximum lifetime
        CATCH_REQUIRE(info.f_ttl == 60);
        CATCH_REQUIRE(snapdev::quiet_floating_point_equal(info.f_cost, 0.67f));
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: verify decaying happens (slow)")
    {
        constexpr int const     VALUE_COUNT = 100;

        magpie::options opts;
        opts.set_cleanup_interval(5);
        opts.set_update_decay(1, 1);
        opts.set_touch_decay(1, 1);

        magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
        opts.set_eviction_policy(ep);

        magpie::cache<int, int> cache(opts);

        int updates[VALUE_COUNT] = {};
        int touches[VALUE_COUNT] = {};
        bool update_decayed(false);
        bool touch_decayed(false);
        do
        {
            for(int k(0); k < 100; ++k)
            {
                magpie::set_result_t status(cache.set(k, rand()));
                if(status != magpie::set_result_t::SET_RESULT_ADDED
                && status != magpie::set_result_t::SET_RESULT_UPDATED)
                {
                    CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
                }
                ++updates[k];
                if((rand() & 1) != 0)
                {
                    std::optional<int> v(cache.get(k));
                    CATCH_REQUIRE(v.has_value()); // we just set, it should always be defined
                    ++touches[k];
                }

                magpie::metadata_t info(cache.get_metadata(k));
                CATCH_REQUIRE(info.is_defined());
                CATCH_REQUIRE(info.f_update_count <= updates[k]);
                CATCH_REQUIRE(info.f_touch_count <= touches[k]);

                if(updates[k] - info.f_update_count > 10)
                {
                    update_decayed = true;
                }
                if(touches[k] - info.f_touch_count > 10)
                {
                    touch_decayed = true;
                }
            }
        }
        while(!update_decayed || !touch_decayed);
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache: verify decaying happens (fast)")
    {
        constexpr int const     VALUE_COUNT = 100;

        magpie::options opts;
        opts.set_cleanup_interval(5);
        opts.set_update_decay(100, 1); // this allows us to have a decay larger than the current counter and end up using the 0
        opts.set_touch_decay(100, 1);

        magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
        opts.set_eviction_policy(ep);

        magpie::cache<int, int> cache(opts);

        int updates[VALUE_COUNT] = {};
        int touches[VALUE_COUNT] = {};
        bool update_decayed(false);
        bool touch_decayed(false);
        do
        {
            for(int k(0); k < 100; ++k)
            {
                magpie::set_result_t status(cache.set(k, rand()));
                if(status != magpie::set_result_t::SET_RESULT_ADDED
                && status != magpie::set_result_t::SET_RESULT_UPDATED)
                {
                    CATCH_REQUIRE(status == magpie::set_result_t::SET_RESULT_ADDED);
                }
                ++updates[k];
                if((rand() & 1) != 0)
                {
                    std::optional<int> v(cache.get(k));
                    CATCH_REQUIRE(v.has_value()); // we just set, it should always be defined
                    ++touches[k];
                }

                magpie::metadata_t info(cache.get_metadata(k));
                CATCH_REQUIRE(info.is_defined());
                CATCH_REQUIRE(info.f_update_count <= updates[k]);
                CATCH_REQUIRE(info.f_touch_count <= touches[k]);

                if(updates[k] - info.f_update_count > 10)
                {
                    update_decayed = true;
                }
                if(touches[k] - info.f_touch_count > 10)
                {
                    touch_decayed = true;
                }
            }
        }
        while(!update_decayed || !touch_decayed);
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

            CATCH_REQUIRE_THROWS_MATCHES(
                      cache[std::string()] = v
                    , magpie::empty_key_not_accepted
                    , Catch::Matchers::ExceptionMessage(
                              "magpie_exception: this cache was setup to not accept the empty key."));
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("cache_error: verify a dereference on a cache_ref throws when the value is not defined")
    {
        for(std::size_t lock_model(0); lock_model < std::size(g_lock_models); ++lock_model)
        {
            magpie::options opts;
            opts.set_lock_model(g_lock_models[lock_model]);

            magpie::cache<int, int> cache(opts);

            std::string const v(SNAP_CATCH2_NAMESPACE::random_string(1, 32, SNAP_CATCH2_NAMESPACE::character_t::CHARACTER_UNICODE));
            CATCH_REQUIRE_THROWS_MATCHES(
                      *cache[rand()]
                    , magpie::value_not_found
                    , Catch::Matchers::ExceptionMessage(
                              "magpie_exception: value not currently set."));
        }
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et
