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


/** \file
 * \brief The core implementation of the magpie cache.
 *
 * This file implements the core functions of the magpie cache functionality.
 */



// self
//
//#include    <magpie/cache_impl.h>
//
//#include    <magpie/cache.h>
//#include    <magpie/cleanup_runner.h>
//#include    <magpie/lock_mutex.h>
//#include    <magpie/lock_recursive_rwlock.h>
//#include    <magpie/lock_rwlock.h>



// cppthread
//
//#include    <cppthread/thread.h>


// murmur3
//
//#include    <murmur3/murmur3.h>


// C++
//
#include    <cstdint>
#include    <memory>



namespace magpie
{



typedef std::uint32_t                   linear_t;

constexpr linear_t const                LINEAR_ACCESSES  = 0x0001;
constexpr linear_t const                LINEAR_RECENCY   = 0x0002;
constexpr linear_t const                LINEAR_BYTE_SIZE = 0x0004;
constexpr linear_t const                LINEAR_AGE       = 0x0008;

constexpr double const                  EVICTION_TRIGGER_DEFAULT = 0.95;
constexpr double const                  EVICTION_REDUCTION_DEFAULT = 0.9;



class eviction_policy
{
public:
    typedef std::shared_ptr<eviction_policy>        pointer_t;

    virtual                 ~eviction_policy() = default;

    // per cache data
    //
    void                    set_size_eviction_trigger_percent(double percent);
    double                  get_size_eviction_trigger_percent() const;
    std::size_t             get_size_eviction_trigger_limit(std::size_t count_limit) const;
    void                    set_size_eviction_reduction_percent(double percent);
    double                  get_size_eviction_reduction_percent() const;
    std::size_t             get_size_eviction_reduction_limit(std::size_t count_limit) const;

    void                    set_access_count_weight(double weight);
    double                  get_access_count_weight() const;
    void                    set_recency_weight(double weight);
    double                  get_recency_weight() const;
    void                    set_recency_decay_half_time(double decay_half_time);
    double                  get_recency_decay_half_time() const;
    void                    set_allocated_weight(double weight);
    double                  get_allocated_weight() const;
    void                    set_age_weight(double weight);
    double                  get_age_weight() const;
    void                    set_age_decay_half_time(double decay_half_time);
    double                  get_age_decay_half_time() const;
    void                    set_random_weight(double weight);
    double                  get_random_weight() const;

    void                    set_linear_normalization(linear_t linear_normalization);
    linear_t                get_linear_normalization() const;

    // per cycle data
    //
    void                    set_cycle_started_on(double cycle_started_on);
    double                  get_cycle_started_on() const;
    void                    set_maximum_access_count(double maximum_access_count);
    double                  get_maximum_access_count() const;
    void                    set_maximum_allocated(double maximum_allocated);
    double                  get_maximum_allocated() const;
    void                    set_oldest_item_created_on(double created_on);
    double                  get_oldest_item_created_on() const;
    void                    set_most_recent_item_update(double updated_on);
    double                  get_most_recent_item_update() const;
    void                    set_most_recent_item_touch(double touched_on);
    double                  get_most_recent_item_touch() const;
    void                    set_oldest_item_update(double oldest_update);
    double                  get_oldest_item_update() const;
    void                    set_oldest_item_touch(double oldest_update);
    double                  get_oldest_item_touch() const;
    void                    set_highest_item_turn(double turn);
    double                  get_highest_item_turn() const;

    // per item data
    //
    void                    set_access_count(double access_count);
    double                  get_access_count() const;
    void                    set_last_updated_on(double last_updated_on);
    double                  get_last_updated_on() const;
    void                    set_last_touched_on(double last_touched_on);
    double                  get_last_touched_on() const;
    void                    set_allocated(double allocated);
    double                  get_allocated() const;
    void                    set_item_created_on(double created_on);
    double                  get_item_created_on() const;
    void                    set_item_timeout(double timeout);
    double                  get_item_timeout() const;
    void                    set_item_turn(double turn);
    double                  get_item_turn() const;

    // helper functions for computations
    //
    double                  normalized_accesses(double default_value = 0.0) const;
    double                  normalized_linear_accesses(double default_value = 0.0) const;
    double                  normalized_log_saturate_accesses() const;

    double                  normalized_recency(double default_value = 0.0) const;
    double                  normalized_linear_recency(double default_value = 0.0) const;
    double                  normalized_exp_decay_recency() const;

    double                  normalized_allocated(double default_value = 0.0) const;
    double                  normalized_linear_allocated(double default_value = 0.0) const;
    double                  normalized_log_saturate_allocated() const;

    double                  normalized_age(double default_value = 0.0) const;
    double                  normalized_linear_age(double default_value = 0.0) const;
    double                  normalized_exp_decay_age() const;

    double                  normalized_survival(double default_value = 0.0) const;
    double                  normalized_linear_survival(double default_value = 0.0) const;
    double                  normalized_exp_decay_survival() const;

    double                  normalized_random() const;

    // the actual function used to compute the final weight
    //
    virtual double          compute_weight() const = 0;

protected:
    // set of variable set once on initialization
    //
    double                  f_size_eviction_trigger_percent = EVICTION_TRIGGER_DEFAULT;
    double                  f_size_eviction_reduction_percent = EVICTION_REDUCTION_DEFAULT;
    double                  f_access_count_weight = 1.0;
    double                  f_recency_weight = 1.0;
    double                  f_recency_decay_half_time = 3600.0; // 1 hour
    double                  f_allocated_weight = 1.0;
    double                  f_age_weight = 1.0;
    double                  f_age_decay_half_time = 86400.0; // 1 day
    double                  f_random_weight = 1.0;
    linear_t                f_linear_normalization = LINEAR_ACCESSES | LINEAR_RECENCY | LINEAR_BYTE_SIZE | LINEAR_AGE;

    // set of variables immutable during one computing cycle
    //
    double                  f_cycle_started_on = 0.0;           // timestamp when weight computations started
    double                  f_maximum_access_count = 0.0;       // from item with largest access count (approx.)
    double                  f_maximum_allocated = 0.0;          // from item with largest memory byte size
    double                  f_oldest_item_created_on = 0.0;     // timestamp of when the oldest item was created
    double                  f_most_recent_item_update = 0.0;    // timestamp of the most recent item update
    double                  f_most_recent_item_touch = 0.0;     // timestamp of the most recent item touch
    double                  f_oldest_item_update = 0.0;         // timestamp of the item with the oldest f_last_updated_on
    double                  f_oldest_item_touch = 0.0;          // timestamp of the item with the oldest f_last_touched_on
    double                  f_highest_item_turn = 0.0;          // number of times the eviction algorithm ran against this item

    // set of variables set for each item
    //
    double                  f_access_count = 0.0;               // the item access count, at least 1
    double                  f_item_created_on = 0.0;            // timestamp when object was first inserted in cache
    double                  f_last_updated_on = 0.0;            // timestamp of last set()
    double                  f_last_touched_on = 0.0;            // timestamp of last get()
    double                  f_item_timeout = 0.0;               // timestamp when the item is to timeout
    double                  f_allocated = 0.0;                  // byte size of this item
    double                  f_item_turn = 0.0;                  // number of time the eviction algorithm ran against this item
};


class eviction_policy_add
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};


typedef eviction_policy_add   eviction_policy_default;


class eviction_policy_multiply
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};


class eviction_policy_minimum
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};


class eviction_policy_maximum
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};


class eviction_policy_geometric_mean
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};


class eviction_policy_harmonic_mean
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};


class eviction_policy_cost_benefit_ratio
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};



} // namespace magpie
// vim: ts=4 sw=4 et
