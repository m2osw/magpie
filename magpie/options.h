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

// self
//
#include    <magpie/eviction_policy.h>



// C++
//
#include    <cstdint>


// C
//
#include    <time.h>



namespace magpie
{



typedef std::int32_t            duration_t;

constexpr duration_t const      NO_TTL = -2;
constexpr duration_t const      DEFAULT_TTL = -1;
constexpr duration_t const      NO_MAXIMUM_LIFETIME = 0;
constexpr duration_t const      DEFAULT_CLEANUP_INTERVAL = 3600; // 1h

constexpr std::size_t const     NO_LIMIT = 0;
constexpr std::size_t const     NO_DECAY = 0;
constexpr std::size_t const     DEFAULT_BUCKET_COUNT = 10;
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
    void                    set_eviction_policy(eviction_policy::pointer_t policy);
    eviction_policy::pointer_t
                            get_eviction_policy() const;

    void                    set_allow_empty_key(bool allowed);
    bool                    get_allow_empty_key() const;

    void                    set_refuse_data_once_full(bool refuse);
    bool                    get_refuse_data_once_full() const;

    void                    set_maximum_object_size(std::size_t maximum_object_size);
    std::size_t             get_maximum_object_size() const;

    void                    set_capacity_limit(std::size_t maximum_allocated);
    std::size_t             get_capacity_limit() const;

    void                    set_maximum_access_count(std::size_t maximum_count);
    std::size_t             get_maximum_access_count() const;

    void                    set_access_count_decay(std::size_t decay);
    std::size_t             get_access_count_decay() const;

    void                    set_count_limit(std::size_t count_limit);
    std::size_t             get_count_limit() const;

    void                    set_bucket_count(std::size_t count);
    std::size_t             get_bucket_count() const;

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

private:
    eviction_policy::pointer_t
                            f_eviction_policy = eviction_policy::pointer_t();

    bool                    f_allow_empty_key = false;
    bool                    f_refuse_data_once_full = true;
    std::size_t             f_maximum_object_size = NO_LIMIT;
    std::size_t             f_maximum_allocated = NO_LIMIT;
    std::size_t             f_maximum_access_count = NO_LIMIT;
    std::size_t             f_access_count_decay = NO_DECAY;
    std::size_t             f_count_limit = NO_LIMIT;
    std::size_t             f_bucket_count = DEFAULT_BUCKET_COUNT;
    duration_t              f_default_ttl = NO_TTL;
    duration_t              f_maximum_lifetime = NO_MAXIMUM_LIFETIME;
    duration_t              f_cleanup_interval = DEFAULT_CLEANUP_INTERVAL;
    lock_model_t            f_lock_model = lock_model_t::LOCK_MODEL_MUTEX;
    timespec                f_wait_lock_for_stale_data_deletion = { 0, 4 };
    stale_data_t            f_stale_data = stale_data_t::STALE_DATA_DELETE;
};



} // namespace magpie
// vim: ts=4 sw=4 et
