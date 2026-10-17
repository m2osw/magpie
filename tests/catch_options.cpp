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
#include    <magpie/options.h>

#include    <magpie/exception.h>


// snapdev
//
#include    <snapdev/enum_class_math.h>
#include    <snapdev/timespec_ex.h>


// last include
//
#include    <snapdev/poison.h>



CATCH_TEST_CASE("options", "[options]")
{
    CATCH_START_SECTION("options: verify options defaults")
    {
        magpie::options opts;

        CATCH_REQUIRE_FALSE(opts.get_least_recently_used());
        CATCH_REQUIRE_FALSE(opts.get_least_frequently_used());
        CATCH_REQUIRE_FALSE(opts.get_allow_empty_key());
        CATCH_REQUIRE(opts.get_capacity() == 0);
        CATCH_REQUIRE(opts.get_count() == 0);
        CATCH_REQUIRE(opts.get_default_ttl() == magpie::NO_TTL);
        CATCH_REQUIRE(opts.get_cleanup_interval() == 3600);
        CATCH_REQUIRE(opts.get_lock_model() == magpie::lock_model_t::LOCK_MODEL_MUTEX);
        snapdev::timespec_ex wait{ 0, 4 };
        CATCH_REQUIRE(opts.get_wait_lock_for_stale_data_deletion() == wait);
        CATCH_REQUIRE(opts.get_stale_data() == magpie::stale_data_t::STALE_DATA_DELETE);
        CATCH_REQUIRE(opts.get_bucket_count() == 10);
    }
    CATCH_END_SECTION()


    CATCH_START_SECTION("options: verify options can be changed")
    {
        magpie::options opts;

        {
            CATCH_REQUIRE_FALSE(opts.get_least_recently_used());
            opts.set_least_recently_used(true);
            CATCH_REQUIRE(opts.get_least_recently_used());
            opts.set_least_recently_used(false);
            CATCH_REQUIRE_FALSE(opts.get_least_recently_used());
        }

        {
            CATCH_REQUIRE_FALSE(opts.get_least_frequently_used());
            opts.set_least_frequently_used(true);
            CATCH_REQUIRE(opts.get_least_frequently_used());
            opts.set_least_frequently_used(false);
            CATCH_REQUIRE_FALSE(opts.get_least_frequently_used());
        }

        {
            CATCH_REQUIRE_FALSE(opts.get_allow_empty_key());
            opts.set_allow_empty_key(true);
            CATCH_REQUIRE(opts.get_allow_empty_key());
            opts.set_allow_empty_key(false);
            CATCH_REQUIRE_FALSE(opts.get_allow_empty_key());
        }

        for(std::size_t capacity(0); capacity < 1'000'000; capacity += rand() % 1'000)
        {
            opts.set_capacity(capacity);
            CATCH_REQUIRE(opts.get_capacity() == capacity);
        }

        for(std::size_t count(0); count < 1'000'000; count += rand() % 1'000)
        {
            opts.set_count(count);
            CATCH_REQUIRE(opts.get_count() == count);
        }

        for(magpie::duration_t duration(1); duration < 1'000'000; duration += rand() % 1'000)
        {
            opts.set_default_ttl(duration);
            CATCH_REQUIRE(opts.get_default_ttl() == duration);
        }

        // specifically test the special values
        {
            opts.set_default_ttl(magpie::NO_TTL);
            CATCH_REQUIRE(opts.get_default_ttl() == magpie::NO_TTL);

            opts.set_default_ttl(magpie::DEFAULT_TTL);
            CATCH_REQUIRE(opts.get_default_ttl() == magpie::DEFAULT_TTL);
        }

        for(magpie::duration_t duration(1); duration < 1'000'000; duration += rand() % 1'000)
        {
            opts.set_cleanup_interval(duration);
            CATCH_REQUIRE(opts.get_cleanup_interval() == duration);
        }

        for(magpie::lock_model_t model(magpie::lock_model_t::LOCK_MODEL_MUTEX);
            model <= magpie::lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK;
            ++model)
        {
            opts.set_lock_model(model);
            CATCH_REQUIRE(opts.get_lock_model() == model);
        }

        for(int count(0); count < 10; ++count)
        {
            timespec wait{ rand(), rand() % 1'000'000'000 };
            opts.set_wait_lock_for_stale_data_deletion(wait);
            CATCH_REQUIRE(opts.get_wait_lock_for_stale_data_deletion() == wait);
        }

        for(magpie::stale_data_t mode(magpie::stale_data_t::STALE_DATA_IGNORE);
            mode <= magpie::stale_data_t::STALE_DATA_RETURN;
            ++mode)
        {
            opts.set_stale_data(mode);
            CATCH_REQUIRE(opts.get_stale_data() == mode);
        }

        for(std::size_t count(1); count < magpie::MAX_BUCKET_COUNT; count += rand())
        {
            opts.set_bucket_count(count);
            CATCH_REQUIRE(opts.get_bucket_count() == count);
        }
        opts.set_bucket_count(magpie::MAX_BUCKET_COUNT);
        CATCH_REQUIRE(opts.get_bucket_count() == magpie::MAX_BUCKET_COUNT);
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("options_error", "[options][error]")
{
    CATCH_START_SECTION("options_error: verify that the TTL cannot be a negative number")
    {
        magpie::options opts;

        for(magpie::duration_t duration(-3); duration > -100; --duration)
        {
            CATCH_REQUIRE_THROWS_MATCHES(
                      opts.set_default_ttl(duration)
                    , magpie::out_of_range
                    , Catch::Matchers::ExceptionMessage(
                              "out_of_range: options::set_default_ttl(): ttl must be a positive number, magpie::NO_TTL, or magpie::DEFAULT_TTL."));
        }
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("options_error: verify bucket minimum & maximum")
    {
        magpie::options opts;

        CATCH_REQUIRE_THROWS_MATCHES(
                  opts.set_bucket_count(0)
                , magpie::out_of_range
                , Catch::Matchers::ExceptionMessage(
                          "out_of_range: bucket count out of range."));

        CATCH_REQUIRE_THROWS_MATCHES(
                  opts.set_bucket_count(magpie::MAX_BUCKET_COUNT + 1)
                , magpie::out_of_range
                , Catch::Matchers::ExceptionMessage(
                          "out_of_range: bucket count out of range."));
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et
