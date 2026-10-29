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


// last include
//
#include    <snapdev/poison.h>



namespace
{






} // no name namespace



CATCH_TEST_CASE("add", "[add]")
{
    CATCH_START_SECTION("add: try inc() and dec() [mutex based]")
    {
        constexpr std::size_t const     VALUE_COUNT = 100;

        magpie::options opts;
        magpie::cache<int, int> cache(opts);

        int values[VALUE_COUNT];
        for(std::size_t idx(0); idx < VALUE_COUNT; ++idx)
        {
            values[idx] = rand();
            if((rand() & 1) == 0)
            {
                magpie::set_result_t r(cache.set(idx, values[idx]));
                CATCH_REQUIRE(r == magpie::set_result_t::SET_RESULT_ADDED);
            }
            else
            {
                // still missing
                //
                std::optional<int> m(cache.get(idx));
                CATCH_REQUIRE_FALSE(m.has_value());

                // do an add() which defines the value
                //
                int const r(cache.add(idx, values[idx]));
                CATCH_REQUIRE(r == values[idx]);

                // now the value exists
                //
                std::optional<int> v(cache.get(idx));
                CATCH_REQUIRE(v.has_value());
                CATCH_REQUIRE(v.value() == values[idx]);
            }
        }

        for(std::size_t inc(0); inc < VALUE_COUNT; ++inc)
        {
            std::optional<int> v(cache.get(inc));
            CATCH_REQUIRE(v.has_value());
            CATCH_REQUIRE(v.value() == values[inc]);

            int const inc_result(cache.inc(inc));

            std::optional<int> iv(cache.get(inc));
            CATCH_REQUIRE(iv.has_value());
            CATCH_REQUIRE(values[inc] + 1 == iv.value());
            CATCH_REQUIRE(values[inc] + 1 == inc_result);

            int const dec_result(cache.dec(inc));

            std::optional<int> dv(cache.get(inc));
            CATCH_REQUIRE(dv.has_value());
            CATCH_REQUIRE(values[inc] == dv.value());
            CATCH_REQUIRE(values[inc] == dec_result);

            int const diff(rand());
            int const add_result(cache.add(inc, diff));

            std::optional<int> av(cache.get(inc));
            CATCH_REQUIRE(av.has_value());
            CATCH_REQUIRE(values[inc] + diff == av.value());
            CATCH_REQUIRE(values[inc] + diff == add_result);

            // try with a cache_ref as well
            //
            CATCH_REQUIRE(values[inc] == cache[inc] - diff);
            cache[inc] -= diff;
            std::optional<int> omv(cache.get(inc));
            CATCH_REQUIRE(omv.has_value());
            CATCH_REQUIRE(values[inc] == omv.value());

            CATCH_REQUIRE(values[inc] + diff == cache[inc] + diff);
            cache[inc] += diff;
            std::optional<int> opv(cache.get(inc));
            CATCH_REQUIRE(opv.has_value());
            CATCH_REQUIRE(values[inc] + diff == opv.value());
        }

        // done, everything gets destroyed
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("add: try inc() and dec() [recursive_rwlock based]")
    {
        constexpr std::size_t const     VALUE_COUNT = 100;

        magpie::options opts;
        opts.set_lock_model(magpie::lock_model_t::LOCK_MODEL_RECURSIVE_RWLOCK);
        magpie::cache<int, int> cache(opts);

        int values[VALUE_COUNT];
        for(std::size_t idx(0); idx < VALUE_COUNT; ++idx)
        {
            values[idx] = rand();
            magpie::set_result_t r(cache.set(idx, values[idx]));
            CATCH_REQUIRE(r == magpie::set_result_t::SET_RESULT_ADDED);
        }

        for(std::size_t inc(0); inc < VALUE_COUNT; ++inc)
        {
            std::optional<int> v(cache.get(inc));
            CATCH_REQUIRE(v.has_value());
            CATCH_REQUIRE(v.value() == values[inc]);

            int const inc_result(cache.inc(inc));

            std::optional<int> iv(cache.get(inc));
            CATCH_REQUIRE(iv.has_value());
            CATCH_REQUIRE(values[inc] + 1 == iv.value());
            CATCH_REQUIRE(values[inc] + 1 == inc_result);

            int const dec_result(cache.dec(inc));

            std::optional<int> dv(cache.get(inc));
            CATCH_REQUIRE(dv.has_value());
            CATCH_REQUIRE(values[inc] == dv.value());
            CATCH_REQUIRE(values[inc] == dec_result);

            int const diff(rand());
            int const add_result(cache.add(inc, diff));

            std::optional<int> av(cache.get(inc));
            CATCH_REQUIRE(av.has_value());
            CATCH_REQUIRE(values[inc] + diff == av.value());
            CATCH_REQUIRE(values[inc] + diff == add_result);
        }

        // done, everything gets destroyed
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("add_error", "[add][error]")
{
    CATCH_START_SECTION("add_error: expecting exception when using inc() and dec() with a pure rwlock")
    {
        magpie::options opts;
        opts.set_lock_model(magpie::lock_model_t::LOCK_MODEL_RWLOCK);
        magpie::cache<int, int> cache(opts);

        CATCH_REQUIRE_THROWS_MATCHES(
                      cache.add(101, 25, true)
                    , magpie::incompatible_lock
                    , Catch::Matchers::ExceptionMessage(
                              "magpie_exception: the add(), inc(), dec() are not available with lock model \"rwlock\"."));

        CATCH_REQUIRE_THROWS_MATCHES(
                      cache.inc(101)
                    , magpie::incompatible_lock
                    , Catch::Matchers::ExceptionMessage(
                              "magpie_exception: the add(), inc(), dec() are not available with lock model \"rwlock\"."));

        CATCH_REQUIRE_THROWS_MATCHES(
                      cache.dec(101)
                    , magpie::incompatible_lock
                    , Catch::Matchers::ExceptionMessage(
                              "magpie_exception: the add(), inc(), dec() are not available with lock model \"rwlock\"."));

        // done, everything gets destroyed
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et
