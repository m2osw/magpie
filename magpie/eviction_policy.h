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
 * \brief The implementation of the eviction policy.
 *
 * The cache can be setup with an eviction policy. This allows for the
 * cache to be cleaned up automatically using a background thread.
 *
 * The eviction policy defines how to determine which items to evict.
 * Note that the code here only defines the policy which main point
 * is to compute a weight for each item in the cache and use that to
 * know which item needs to be deleted when a limit is reached.
 */


// self
//
#include    <magpie/exception.h>
#include    <magpie/utils.h>



// C++
//
#include    <atomic>
#include    <cmath>
#include    <memory>



namespace magpie
{



typedef std::uint32_t                   linear_t;

constexpr linear_t const                LINEAR_ACCESSES  = 0x0001;
constexpr linear_t const                LINEAR_UPDATES   = 0x0002;
constexpr linear_t const                LINEAR_TOUCHES   = 0x0004;
constexpr linear_t const                LINEAR_RECENCY   = 0x0008;
constexpr linear_t const                LINEAR_ALLOCATED = 0x0010;
constexpr linear_t const                LINEAR_AGE       = 0x0020;
constexpr linear_t const                LINEAR_SURVIVAL  = 0x0040;

constexpr double const                  EVICTION_TRIGGER_DEFAULT   = 0.95;
constexpr double const                  EVICTION_REDUCTION_DEFAULT = 0.9;

constexpr double const                  NO_COST = 1.0;


class eviction_profile
{
public:
    std::size_t get_size_eviction_trigger_limit(std::size_t count_limit) const
    {
        return static_cast<std::size_t>(std::floor(count_limit * f_size_eviction_trigger_percent.load(std::memory_order_relaxed)));
    }

    std::size_t get_size_eviction_reduction_limit(std::size_t count_limit) const
    {
        return static_cast<std::size_t>(std::floor(count_limit * f_size_eviction_reduction_percent.load(std::memory_order_relaxed)));
    }

    void set_size_eviction_trigger_percent(double percent)
    {
        f_size_eviction_trigger_percent.store(percent, std::memory_order_relaxed);
    }

    double get_size_eviction_trigger_percent() const
    {
        return f_size_eviction_trigger_percent.load(std::memory_order_relaxed);
    }

    void set_size_eviction_reduction_percent(double percent)
    {
        f_size_eviction_reduction_percent.store(percent, std::memory_order_relaxed);
    }

    double get_size_eviction_reduction_percent() const
    {
        return f_size_eviction_reduction_percent.load(std::memory_order_relaxed);
    }

    void set_access_count_weight(double weight)
    {
        f_access_count_weight.store(weight, std::memory_order_relaxed);
    }

    double get_access_count_weight() const
    {
        return f_access_count_weight.load(std::memory_order_relaxed);
    }

    void set_update_count_weight(double weight)
    {
        f_update_count_weight.store(weight, std::memory_order_relaxed);
    }

    double get_update_count_weight() const
    {
        return f_update_count_weight.load(std::memory_order_relaxed);
    }

    void set_touch_count_weight(double weight)
    {
        f_touch_count_weight.store(weight, std::memory_order_relaxed);
    }

    double get_touch_count_weight() const
    {
        return f_touch_count_weight.load(std::memory_order_relaxed);
    }

    void set_recency_weight(double weight)
    {
        f_recency_weight.store(weight, std::memory_order_relaxed);
    }

    double get_recency_weight() const
    {
        return f_recency_weight.load(std::memory_order_relaxed);
    }

    void set_recency_write_weight(double weight)
    {
        f_recency_write_weight.store(weight, std::memory_order_relaxed);
    }

    double get_recency_write_weight() const
    {
        return f_recency_write_weight.load(std::memory_order_relaxed);
    }

    void set_recency_read_weight(double weight)
    {
        f_recency_read_weight.store(weight, std::memory_order_relaxed);
    }

    double get_recency_read_weight() const
    {
        return f_recency_read_weight.load(std::memory_order_relaxed);
    }

    void set_recency_decay_half_time(double decay_half_time)
    {
        if(f_recency_decay_half_time <= 0.0)
        {
            throw invalid_parameter("the recency decay half-time cannot be negative or null.");
        }
        f_recency_decay_half_time.store(decay_half_time, std::memory_order_relaxed);
    }

    double get_recency_decay_half_time() const
    {
        return f_recency_decay_half_time.load(std::memory_order_relaxed);
    }

    /** \brief Set the weight used to tweak the cache eviction.
     *
     * This parameter is used by the ADD algorithm if not 0.0. It
     * uses the normalized allocated sizes of items compared against
     * the item with the largest amount of allocated memory.
     *
     * \warning
     * Keep in mind that this weight is \em inverted, meaning that if
     * you want to remove the largest files, you must use a negative
     * weight instead of a positive one.
     *
     * \param[in] weight  Weight used against the allocated memory values.
     */
    void set_allocated_weight(double weight)
    {
        f_allocated_weight.store(weight, std::memory_order_relaxed);
    }

    double get_allocated_weight() const
    {
        return f_allocated_weight.load(std::memory_order_relaxed);
    }

    void set_age_weight(double weight)
    {
        f_age_weight.store(weight, std::memory_order_relaxed);
    }

    double get_age_weight() const
    {
        return f_age_weight.load(std::memory_order_relaxed);
    }

    void set_age_decay_half_time(double decay_half_time)
    {
        if(f_age_decay_half_time <= 0.0)
        {
            throw invalid_parameter("the age decay half-time cannot be negative or null.");
        }
        f_age_decay_half_time.store(decay_half_time, std::memory_order_relaxed);
    }

    double get_age_decay_half_time() const
    {
        return f_age_decay_half_time.load(std::memory_order_relaxed);
    }

    void set_survival_weight(double weight)
    {
        f_survival_weight.store(weight, std::memory_order_relaxed);
    }

    double get_survival_weight() const
    {
        return f_survival_weight.load(std::memory_order_relaxed);
    }

    void set_survival_decay_half_time(double decay_half_time)
    {
        if(f_survival_decay_half_time <= 0.0)
        {
            throw invalid_parameter("the survival decay half-time cannot be negative or null.");
        }
        f_survival_decay_half_time.store(decay_half_time, std::memory_order_relaxed);
    }

    double get_survival_decay_half_time() const
    {
        return f_survival_decay_half_time.load(std::memory_order_relaxed);
    }

    void set_turn_weight(double turn)
    {
        f_turn_weight.store(turn, std::memory_order_relaxed);
    }

    double get_turn_weight() const
    {
        return f_turn_weight.load(std::memory_order_relaxed);
    }

    void set_random_weight(double random)
    {
        f_random_weight.store(random, std::memory_order_relaxed);
    }

    double get_random_weight() const
    {
        return f_random_weight.load(std::memory_order_relaxed);
    }

    void set_linear_normalization(linear_t linear_normalization)
    {
        f_linear_normalization.store(linear_normalization, std::memory_order_relaxed);
    }

    linear_t get_linear_normalization() const
    {
        return f_linear_normalization.load(std::memory_order_relaxed);
    }

private:
    std::atomic<double>     f_size_eviction_trigger_percent = EVICTION_TRIGGER_DEFAULT;
    std::atomic<double>     f_size_eviction_reduction_percent = EVICTION_REDUCTION_DEFAULT;

    std::atomic<double>     f_access_count_weight = 0.0;
    std::atomic<double>     f_update_count_weight = 0.0;
    std::atomic<double>     f_touch_count_weight = 0.0;
    std::atomic<double>     f_recency_weight = 0.0;
    std::atomic<double>     f_recency_write_weight = 0.0;
    std::atomic<double>     f_recency_read_weight = 0.0;
    std::atomic<double>     f_recency_decay_half_time = 3600.0; // 1 hour
    std::atomic<double>     f_allocated_weight = 0.0;
    std::atomic<double>     f_age_weight = 0.0;
    std::atomic<double>     f_age_decay_half_time = 86400.0; // 1 day
    std::atomic<double>     f_survival_weight = 0.0;
    std::atomic<double>     f_survival_decay_half_time = 86400.0; // 1 day
    std::atomic<double>     f_turn_weight = 0.0;
    std::atomic<double>     f_random_weight = 0.0;

    std::atomic<linear_t>   f_linear_normalization = LINEAR_ACCESSES | LINEAR_RECENCY | LINEAR_ALLOCATED | LINEAR_AGE | LINEAR_SURVIVAL;
};


class eviction_cycle
{
public:
    void set_cycle_started_on(double cycle_started_on)
    {
        f_started_on.store(cycle_started_on, std::memory_order_relaxed);
    }

    double get_cycle_started_on() const
    {
        return f_started_on.load(std::memory_order_relaxed);
    }

    double get_cycle_turn() const
    {
        return f_turn.load(std::memory_order_relaxed);
    }

    void set_item_maximum_update_count(double maximum_update_count)
    {
        f_item_maximum_update_count.store(maximum_update_count, std::memory_order_relaxed);
    }

    double get_item_maximum_update_count() const
    {
        return f_item_maximum_update_count.load(std::memory_order_relaxed);
    }

    void set_item_maximum_touch_count(double maximum_touch_count)
    {
        f_item_maximum_touch_count.store(maximum_touch_count, std::memory_order_relaxed);
    }

    double get_item_maximum_touch_count() const
    {
        return f_item_maximum_touch_count.load(std::memory_order_relaxed);
    }

    void set_item_maximum_allocated(double maximum_allocated)
    {
        f_item_maximum_allocated.store(maximum_allocated, std::memory_order_relaxed);
    }

    double get_item_maximum_allocated() const
    {
        return f_item_maximum_allocated.load(std::memory_order_relaxed);
    }

    void set_item_first_created_on(double created_on)
    {
        f_item_first_created_on.store(created_on, std::memory_order_relaxed);
    }

    double get_item_first_created_on() const
    {
        return f_item_first_created_on.load(std::memory_order_relaxed);
    }

    void set_item_oldest_created_on(double created_on)
    {
        f_item_oldest_created_on.store(created_on, std::memory_order_relaxed);
    }

    double get_item_oldest_created_on() const
    {
        return f_item_oldest_created_on.load(std::memory_order_relaxed);
    }

    void set_item_most_recent_update(double updated_on)
    {
        f_item_most_recent_update.store(updated_on, std::memory_order_relaxed);
    }

    double get_item_most_recent_update() const
    {
        return f_item_most_recent_update.load(std::memory_order_relaxed);
    }

    void set_item_most_recent_touch(double touched_on)
    {
        f_item_most_recent_touch.store(touched_on, std::memory_order_relaxed);
    }

    double get_item_most_recent_touch() const
    {
        return f_item_most_recent_touch.load(std::memory_order_relaxed);
    }

    double get_item_most_recent_access() const
    {
        return std::max(f_item_most_recent_update.load(std::memory_order_relaxed)
                      , f_item_most_recent_touch.load(std::memory_order_relaxed));
    }

    void set_item_oldest_update(double oldest_update)
    {
        f_item_oldest_update.store(oldest_update, std::memory_order_relaxed);
    }

    double get_item_oldest_update() const
    {
        return f_item_oldest_update.load(std::memory_order_relaxed);
    }

    void set_item_oldest_touch(double oldest_touch)
    {
        f_item_oldest_touch.store(oldest_touch, std::memory_order_relaxed);
    }

    double get_item_oldest_touch() const
    {
        return f_item_oldest_touch.load(std::memory_order_relaxed);
    }

    double get_item_oldest_access() const
    {
        return std::min(f_item_oldest_update.load(std::memory_order_relaxed)
                      , f_item_oldest_touch.load(std::memory_order_relaxed));
    }

    void set_item_furthest_timeout(double furthest_timeout)
    {
        f_item_furthest_timeout.store(furthest_timeout, std::memory_order_relaxed);
    }

    double get_item_furthest_timeout() const
    {
        return f_item_furthest_timeout.load(std::memory_order_relaxed);
    }

    void set_item_highest_turn(double turn)
    {
        f_item_highest_turn.store(turn, std::memory_order_relaxed);
    }

    double get_item_highest_turn() const
    {
        return f_item_highest_turn.load(std::memory_order_relaxed);
    }

    void new_cycle(
          biased_time_t const & first_item_created_on
        , biased_time_t const & last_item_updated_on
        , biased_time_t const & last_item_touched_on);

private:
    std::atomic<double>     f_started_on = 0.0;                 // timestamp when weight computations started
    std::atomic<double>     f_turn = 0.0;                       // total number of times this eviction ran

    // item min/max
    //
    std::atomic<double>     f_item_maximum_update_count = 0.0;  // from item with largest update count (set)
    std::atomic<double>     f_item_maximum_touch_count = 0.0;   // from item with largest touch count (get)
    std::atomic<double>     f_item_maximum_allocated = 0.0;     // from item with largest memory byte size
    std::atomic<double>     f_item_first_created_on = 0.0;      // timestamp of when the first item was created
    std::atomic<double>     f_item_oldest_created_on = 0.0;     // timestamp of when the oldest item was created
    std::atomic<double>     f_item_most_recent_update = 0.0;    // timestamp of the most recent item update
    std::atomic<double>     f_item_most_recent_touch = 0.0;     // timestamp of the most recent item touch
    std::atomic<double>     f_item_oldest_update = 0.0;         // timestamp of the item with the oldest f_last_updated_on
    std::atomic<double>     f_item_oldest_touch = 0.0;          // timestamp of the item with the oldest f_last_touched_on
    std::atomic<double>     f_item_furthest_timeout = 0.0;      // timestamp of the item with the largest f_timeout
    std::atomic<double>     f_item_highest_turn = 0.0;          // number of times the eviction algorithm ran against this item
};


class eviction_item
{
public:
    void set_update_count(double update_count)
    {
        f_update_count.store(update_count, std::memory_order_relaxed);
    }

    double get_update_count() const
    {
        return f_update_count.load(std::memory_order_relaxed);
    }

    void set_touch_count(double touch_count)
    {
        f_touch_count.store(touch_count, std::memory_order_relaxed);
    }

    double get_touch_count() const
    {
        return f_touch_count.load(std::memory_order_relaxed);
    }

    void set_last_updated_on(double last_updated_on)
    {
        f_last_updated_on.store(last_updated_on, std::memory_order_relaxed);
    }

    double get_last_updated_on() const
    {
        return f_last_updated_on.load(std::memory_order_relaxed);
    }

    void set_last_touched_on(double last_touched_on)
    {
        f_last_touched_on.store(last_touched_on, std::memory_order_relaxed);
    }

    double get_last_touched_on() const
    {
        return f_last_touched_on.load(std::memory_order_relaxed);
    }

    double get_last_accessed_on() const
    {
        return std::max(f_last_updated_on.load(std::memory_order_relaxed)
                      , f_last_touched_on.load(std::memory_order_relaxed));
    }

    void set_allocated(double allocated)
    {
        f_allocated.store(allocated, std::memory_order_relaxed);
    }

    double get_allocated() const
    {
        return f_allocated.load(std::memory_order_relaxed);
    }

    void set_created_on(double created_on)
    {
        f_created_on.store(created_on, std::memory_order_relaxed);
    }

    double get_created_on() const
    {
        return f_created_on.load(std::memory_order_relaxed);
    }

    void set_timeout(double timeout)
    {
        f_timeout.store(timeout, std::memory_order_relaxed);
    }

    double get_timeout() const
    {
        return f_timeout.load(std::memory_order_relaxed);
    }

    void set_cost(double cost)
    {
        f_cost.store(cost, std::memory_order_relaxed);
    }

    double get_cost() const
    {
        return f_cost.load(std::memory_order_relaxed);
    }

    void set_turn(double turn)
    {
        f_turn.store(turn, std::memory_order_relaxed);
    }

    double get_turn() const
    {
        return f_turn.load(std::memory_order_relaxed);
    }

private:
    std::atomic<double>     f_update_count = 0.0;           // the item update count (set)
    std::atomic<double>     f_touch_count = 0.0;            // the item touch count (get)
    std::atomic<double>     f_created_on = 0.0;             // timestamp when object was first inserted in cache
    std::atomic<double>     f_last_updated_on = 0.0;        // timestamp of last set()
    std::atomic<double>     f_last_touched_on = 0.0;        // timestamp of last get()
    std::atomic<double>     f_timeout = 0.0;                // timestamp when the item is to timeout
    std::atomic<double>     f_allocated = 0.0;              // byte size of this item
    std::atomic<double>     f_cost = NO_COST;               // the cost of evicting this item
    std::atomic<double>     f_turn = 0.0;                   // number of time the eviction algorithm ran against this item
};


class eviction_policy
{
public:
    typedef std::shared_ptr<eviction_policy>        pointer_t;

    virtual                     ~eviction_policy() = default;

    eviction_profile const &    profile() const { return f_profile; }
    eviction_profile &          profile()       { return f_profile; }
    eviction_cycle const &      cycle() const   { return f_cycle; }
    eviction_cycle &            cycle()         { return f_cycle; }
    eviction_item const &       item() const    { return f_item; }
    eviction_item &             item()          { return f_item; }

    // helper functions for computations
    //
    double                      normalized_accesses(double default_value = 0.0) const;
    double                      normalized_linear_accesses(double default_value = 0.0) const;
    double                      normalized_log_saturate_accesses() const;

    double                      normalized_updates(double default_value = 0.0) const;
    double                      normalized_linear_updates(double default_value = 0.0) const;
    double                      normalized_log_saturate_updates() const;

    double                      normalized_touches(double default_value = 0.0) const;
    double                      normalized_linear_touches(double default_value = 0.0) const;
    double                      normalized_log_saturate_touches() const;

    double                      normalized_recency(double default_value = 0.0) const;
    double                      normalized_linear_recency(double default_value = 0.0) const;
    double                      normalized_exp_decay_recency() const;

    double                      normalized_recency_write(double default_value = 0.0) const;
    double                      normalized_linear_recency_write(double default_value = 0.0) const;
    double                      normalized_exp_decay_recency_write() const;

    double                      normalized_recency_read(double default_value = 0.0) const;
    double                      normalized_linear_recency_read(double default_value = 0.0) const;
    double                      normalized_exp_decay_recency_read() const;

    double                      normalized_allocated(double default_value = 0.0) const;
    double                      normalized_linear_allocated(double default_value = 0.0) const;
    double                      normalized_log_saturate_allocated() const;

    double                      normalized_age(double default_value = 0.0) const;
    double                      normalized_linear_age(double default_value = 0.0) const;
    double                      normalized_exp_decay_age() const;

    double                      normalized_survival(double default_value = 0.0) const;
    double                      normalized_linear_survival(double default_value = 0.0) const;
    double                      normalized_exp_decay_survival() const;

    double                      normalized_turn(double default_value = 0.0) const;

    double                      normalized_random() const;

    // the actual function used to compute the final weight
    //
    virtual double              compute_weight() const = 0;

private:
    // variables are organized by category which makes them easier
    // to manage:
    //
    // * profile_t -- what you setup on initialization; you can change those
    //   values later, although in most cases it's configured once
    // * eviction_cycle_t -- what the cache_impl sets up when starting a
    //   new eviction cycle
    // * item_t -- what the cache_impl sets up when ready to compute the
    //   weight for an item
    //
    eviction_profile            f_profile = eviction_profile();
    eviction_cycle              f_cycle = eviction_cycle();
    eviction_item               f_item = eviction_item();
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


class eviction_policy_slru
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;

    void                    set_minimum_touch_count(double count)  { f_minimum_touch_count.store(count, std::memory_order_relaxed); }
    double                  get_minimum_touch_count() const        { return f_minimum_touch_count.load(std::memory_order_relaxed); }

private:
    std::atomic<double>     f_minimum_touch_count = 1.0;
};


class eviction_policy_gdsf
    : public eviction_policy
{
public:
    virtual double          compute_weight() const override;
};



} // namespace magpie
// vim: ts=4 sw=4 et
