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
#pragma once

/** \file
 * \brief The cache setup has to be done using these options.
 *
 * The cache must be initialized in the constructor because once
 * initialized, changes to the settings are not permitted.
 */

// C++
//
#include    <cstdint>


// C
//
#include    <time.h>



namespace magpie
{



typedef std::int64_t            duration_t;

constexpr duration_t const      NO_TTL = -2;
constexpr duration_t const      DEFAULT_TTL = -1;
constexpr duration_t const      NO_MAXIMUM_LIFETIME = 0;

constexpr std::size_t const     MAX_BUCKET_COUNT = 10'000;
constexpr std::int64_t const    EVICT_SWEEP_STRIDE = 10'007;

constexpr std::int64_t const    KEY_SEED1 = 0x952A7219F1248BC5;
constexpr std::int64_t const    KEY_SEED2 = 0xA81F572192741BE4;


enum class lock_model_t
{
    LOCK_MODEL_MUTEX,
    LOCK_MODEL_RWLOCK,
    LOCK_MODEL_RECURSIVE_RWLOCK,
};


enum class stale_data_t
{
    STALE_DATA_IGNORE,
    STALE_DATA_DELETE,
    STALE_DATA_RETURN,
};


class options
{
public:
    void                    set_least_recently_used(bool const & lru);
    bool                    get_least_recently_used() const;

    void                    set_least_frequently_used(bool const & lfu);
    bool                    get_least_frequently_used() const;

    void                    set_allow_empty_key(bool allowed);
    bool                    get_allow_empty_key() const;

    void                    set_capacity(std::size_t maximum_byte_size);
    std::size_t             get_capacity() const;

    void                    set_count(std::size_t maximum_count);
    std::size_t             get_count() const;

    void                    set_default_ttl(duration_t ttl);
    duration_t              get_default_ttl() const;

    void                    set_maximum_lifetime(duration_t max);
    duration_t              get_maximum_lifetime() const;

    void                    set_cleanup_interval(duration_t interval);
    duration_t              get_cleanup_interval() const;

    void                    set_lock_model(lock_model_t model);
    lock_model_t            get_lock_model() const;

    void                    set_wait_lock_for_stale_data_deletion(timespec const & max_wait);
    timespec const &        get_wait_lock_for_stale_data_deletion() const;

    void                    set_stale_data(stale_data_t mode);
    stale_data_t            get_stale_data() const;

    void                    set_bucket_count(std::size_t count);
    std::size_t             get_bucket_count() const;

    void                    set_refuse_data_once_full(bool refuse);
    bool                    get_refuse_data_once_full() const;

private:
    bool                    f_lru = false;
    bool                    f_lfu = false;
    bool                    f_allow_empty_key = false;
    bool                    f_refuse_data_once_full = false;
    std::size_t             f_maximum_byte_size = 0;
    std::size_t             f_maximum_count = 0;
    std::size_t             f_bucket_count = 10;
    duration_t              f_default_ttl = NO_TTL;
    duration_t              f_maximum_lifetime = NO_MAXIMUM_LIFETIME;
    duration_t              f_cleanup_interval = 3600; // 1h by default
    lock_model_t            f_lock_model = lock_model_t::LOCK_MODEL_MUTEX;
    timespec                f_wait_lock_for_stale_data_deletion = { 0, 4 };
    stale_data_t            f_stale_data = stale_data_t::STALE_DATA_DELETE;
};



} // namespace magpie
// vim: ts=4 sw=4 et
