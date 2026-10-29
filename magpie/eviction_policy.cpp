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
#include    <magpie/cache_impl.h>

#include    <magpie/cache.h>
#include    <magpie/cleanup_runner.h>
#include    <magpie/lock_mutex.h>
#include    <magpie/lock_recursive_rwlock.h>
#include    <magpie/lock_rwlock.h>
#include    <magpie/utils.h>



// cppthread
//
#include    <cppthread/thread.h>


// murmur3
//
#include    <murmur3/murmur3.h>


// C++
//
#include    <cstring>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{




/////////////////////// CYCLE ///////////////////////

void eviction_cycle::new_cycle(
      biased_time_t const & first_item_created_on
    , biased_time_t const & last_item_touched_on)
{
    f_started_on = biased_now();
    ++f_turn;

    f_item_maximum_update_count = 0.0;
    f_item_maximum_touch_count = 0.0;
    f_item_maximum_allocated = 1.0;
    f_item_oldest_created_on = std::numeric_limits<biased_time_t>::max();
    f_item_most_recent_update = first_item_created_on;
    f_item_most_recent_touch = last_item_touched_on;
    f_item_oldest_update = std::numeric_limits<biased_time_t>::max();
    f_item_oldest_touch = std::numeric_limits<biased_time_t>::max();
    f_item_furthest_timeout = biased_time_t();
    f_item_highest_turn = 0.0;
}



/////////////////////// NORMALIZATION ///////////////////////

double eviction_policy::normalized_accesses(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_ACCESSES) != 0)
    {
        return normalized_linear_accesses(default_value);
    }
    else
    {
        return normalized_log_saturate_accesses();
    }
}


double eviction_policy::normalized_linear_accesses(double default_value) const
{
    double accesses(cycle().get_item_maximum_update_count()
                  + cycle().get_item_maximum_touch_count());
    if(snapdev::quiet_floating_point_equal(accesses, 0.0))
    {
        return default_value;
    }
    return (f_item.get_update_count() + f_item.get_touch_count()) / accesses;
}


double eviction_policy::normalized_log_saturate_accesses() const
{
    return log(f_item.get_update_count() + f_item.get_touch_count() + 1.0)
         / log(cycle().get_item_maximum_update_count()
             + cycle().get_item_maximum_touch_count()
             + 1.0);
}


double eviction_policy::normalized_updates(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_UPDATES) != 0)
    {
        return normalized_linear_updates(default_value);
    }
    else
    {
        return normalized_log_saturate_updates();
    }
}


double eviction_policy::normalized_linear_updates(double default_value) const
{
    if(snapdev::quiet_floating_point_equal(cycle().get_item_maximum_update_count(), 0.0))
    {
        return default_value;
    }
    return f_item.get_update_count() / cycle().get_item_maximum_update_count();
}


double eviction_policy::normalized_log_saturate_updates() const
{
    return log(f_item.get_update_count() + 1.0) / log(cycle().get_item_maximum_update_count() + 1.0);
}


double eviction_policy::normalized_touches(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_TOUCHES) != 0)
    {
        return normalized_linear_touches(default_value);
    }
    else
    {
        return normalized_log_saturate_touches();
    }
}


double eviction_policy::normalized_linear_touches(double default_value) const
{
    if(snapdev::quiet_floating_point_equal(cycle().get_item_maximum_touch_count(), 0.0))
    {
        return default_value;
    }
    return f_item.get_touch_count() / cycle().get_item_maximum_touch_count();
}


double eviction_policy::normalized_log_saturate_touches() const
{
    return log(f_item.get_touch_count() + 1.0) / log(cycle().get_item_maximum_touch_count() + 1.0);
}


double eviction_policy::normalized_recency(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_RECENCY) != 0)
    {
        return normalized_linear_recency(default_value);
    }
    else
    {
        return normalized_exp_decay_recency();
    }
}


double eviction_policy::normalized_linear_recency(double default_value) const
{
    double const access_age(f_cycle.get_item_most_recent_access() - f_cycle.get_item_oldest_access());
    if(snapdev::quiet_floating_point_equal(access_age, 0.0))
    {
        return default_value;
    }
    return (f_item.get_last_accessed_on() - f_cycle.get_item_oldest_access()) / access_age;
}


double eviction_policy::normalized_exp_decay_recency() const
{
    double const delta_time(f_cycle.get_cycle_started_on() - f_item.get_last_accessed_on());
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / f_profile.get_recency_decay_half_time());
}


double eviction_policy::normalized_recency_write(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_RECENCY) != 0)
    {
        return normalized_linear_recency_write(default_value);
    }
    else
    {
        return normalized_exp_decay_recency_write();
    }
}


double eviction_policy::normalized_linear_recency_write(double default_value) const
{
    double const access_age(f_cycle.get_item_most_recent_update() - f_cycle.get_item_oldest_update());
    if(snapdev::quiet_floating_point_equal(access_age, 0.0))
    {
        return default_value;
    }
    return (f_item.get_last_updated_on() - f_cycle.get_item_oldest_update()) / access_age;
}


double eviction_policy::normalized_exp_decay_recency_write() const
{
    double const delta_time(cycle().get_cycle_started_on() - f_item.get_last_updated_on());
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / f_profile.get_recency_decay_half_time());
}


double eviction_policy::normalized_recency_read(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_RECENCY) != 0)
    {
        return normalized_linear_recency_read(default_value);
    }
    else
    {
        return normalized_exp_decay_recency_read();
    }
}


double eviction_policy::normalized_linear_recency_read(double default_value) const
{
    double const access_age(f_cycle.get_item_most_recent_touch() - f_cycle.get_item_oldest_touch());
    if(snapdev::quiet_floating_point_equal(access_age, 0.0))
    {
        return default_value;
    }
    return (f_item.get_last_touched_on() - f_cycle.get_item_oldest_touch()) / access_age;
}


double eviction_policy::normalized_exp_decay_recency_read() const
{
    double const delta_time(cycle().get_cycle_started_on() - f_item.get_last_touched_on());
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / f_profile.get_recency_decay_half_time());
}


double eviction_policy::normalized_allocated(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_ALLOCATED) != 0)
    {
        return normalized_linear_allocated(default_value);
    }
    else
    {
        return normalized_log_saturate_allocated();
    }
}


/** \brief Linear normalization of the data byte size.
 *
 * If the object should really not stay in the cache if using a lot of
 * memory, you should use the linear algorithm against that parameter.
 *
 * This is useful if gathering the data from a cheap location such as
 * local storage (SSD or HHD) and when memory is a high cost.
 *
 * If you want to use it to keep large values instead, use a negative
 * weight (i.e. -1.0).
 *
 * \return The normalized linear value of the allocated byte size.
 */
double eviction_policy::normalized_linear_allocated(double default_value) const
{
    if(snapdev::quiet_floating_point_equal(f_cycle.get_item_maximum_allocated(), 0.0))
    {
        return default_value;
    }
    return f_item.get_allocated() / f_cycle.get_item_maximum_allocated();
}


/** \brief Log saturate for the data byte size.
 *
 * If the object should stay in the cache even when using a lot of
 * memory, you should use the "log saturate" algorithm against that
 * parameter.
 *
 * This is useful if gathering the data has a high cost (i.e. download
 * from the network).
 *
 * \return The normalized log saturate value of the allocated byte size.
 */
double eviction_policy::normalized_log_saturate_allocated() const
{
    return log(f_item.get_allocated() + 1.0) / log(cycle().get_item_maximum_allocated() + 1.0);
}


double eviction_policy::normalized_age(double default_value) const
{
    if((f_profile.get_linear_normalization() & LINEAR_AGE) != 0)
    {
        return normalized_linear_age(default_value);
    }
    else
    {
        return normalized_exp_decay_age();
    }
}


double eviction_policy::normalized_linear_age(double default_value) const
{
    double const item_age(cycle().get_cycle_started_on() - cycle().get_item_oldest_created_on());
    if(snapdev::quiet_floating_point_equal(item_age, 0.0))
    {
        return default_value;
    }
    return 1.0 - (cycle().get_cycle_started_on() - item().get_created_on()) / item_age;
}


double eviction_policy::normalized_exp_decay_age() const
{
    double const delta_time(cycle().get_cycle_started_on() - item().get_created_on());
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / profile().get_age_decay_half_time());
}


double eviction_policy::normalized_survival(double default_value) const
{
    if((profile().get_linear_normalization() & LINEAR_SURVIVAL) != 0)
    {
        return normalized_linear_survival(default_value);
    }
    else
    {
        return normalized_exp_decay_survival();
    }
}


double eviction_policy::normalized_linear_survival(double default_value) const
{
    double const item_survival(cycle().get_item_furthest_timeout() - cycle().get_cycle_started_on());
    if(snapdev::quiet_floating_point_equal(item_survival, 0.0))
    {
        return default_value;
    }
    return (item().get_timeout() - cycle().get_cycle_started_on()) / item_survival;
}


double eviction_policy::normalized_exp_decay_survival() const
{
    double const delta_time(item().get_timeout() - cycle().get_cycle_started_on());
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / profile().get_survival_decay_half_time());
}


double eviction_policy::normalized_turn(double default_value) const
{
    double const max_turn(cycle().get_item_highest_turn());
    if(snapdev::quiet_floating_point_equal(max_turn, 0.0))
    {
        return default_value;
    }
    return 1.0 - item().get_turn() / max_turn;
}


double eviction_policy::normalized_random() const
{
    return snapdev::random(0.0, 1.0);
}



/////////////////////// COMPUTE WEIGHT IMPLEMENTATIONS ///////////////////////

double eviction_policy_add::compute_weight() const
{
    double weight(0.0);
    if(snapdev::quiet_floating_point_not_equal(profile().get_access_count_weight(), 0.0))
    {
        weight += profile().get_access_count_weight() * normalized_accesses();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_update_count_weight(), 0.0))
    {
        weight += profile().get_update_count_weight() * normalized_updates();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_touch_count_weight(), 0.0))
    {
        weight += profile().get_touch_count_weight() * normalized_touches();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_recency_weight(), 0.0))
    {
        weight += profile().get_recency_weight() * normalized_recency();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_allocated_weight(), 0.0))
    {
        weight += profile().get_allocated_weight() * normalized_allocated();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_age_weight(), 0.0))
    {
        weight += profile().get_age_weight() * normalized_age();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_survival_weight(), 0.0))
    {
        weight += profile().get_survival_weight() * normalized_survival();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_turn_weight(), 0.0))
    {
        weight += profile().get_turn_weight() * normalized_turn();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_random_weight(), 0.0))
    {
        weight += profile().get_random_weight() * normalized_random();
    }
    return weight;
}


double eviction_policy_multiply::compute_weight() const
{
    return normalized_accesses(1.0)
         * normalized_recency(1.0)
         * normalized_allocated(1.0)
         * normalized_age(1.0);
}


double eviction_policy_minimum::compute_weight() const
{
    return std::min({normalized_accesses()
                   , normalized_recency()
                   , normalized_allocated()
                   , normalized_age()});
}


double eviction_policy_maximum::compute_weight() const
{
    return std::max({normalized_accesses()
                   , normalized_recency()
                   , normalized_allocated()
                   , normalized_age()});
}


double eviction_policy_geometric_mean::compute_weight() const
{
    return pow(normalized_accesses(),  profile().get_access_count_weight())
         * pow(normalized_recency(),   profile().get_recency_weight())
         * pow(normalized_allocated(), profile().get_allocated_weight())
         * pow(normalized_age(),       profile().get_age_weight());
}


double eviction_policy_harmonic_mean::compute_weight() const
{
    return (profile().get_access_count_weight() + profile().get_recency_weight() + profile().get_allocated_weight() + profile().get_age_weight())
        / (profile().get_access_count_weight() / normalized_accesses(1.0)
            + profile().get_recency_weight()   / normalized_recency(1.0)
            + profile().get_allocated_weight() / normalized_allocated(1.0)
            + profile().get_age_weight()       / normalized_age(1.0));
}


double eviction_policy_cost_benefit_ratio::compute_weight() const
{
    return (profile().get_access_count_weight() * normalized_accesses()
                + profile().get_recency_weight() * normalized_recency())
                     / pow(item().get_allocated(), profile().get_allocated_weight());
}


double eviction_policy_slru::compute_weight() const
{
    if(item().get_touch_count() <= f_minimum_touch_count)
    {
        // return the oldest first if not old enough
        //
        return normalized_age();
    }
    else
    {
        // return the normal LRU number
        //
        // the +2 is to place all of these after the FIFO items above
        //
        return normalized_recency() + 2.0;
    }
}


double eviction_policy_gdsf::compute_weight() const
{
    double const frequency(normalized_accesses());
    double const cost(item().get_cost());
    double const log_size(log(item().get_allocated() + 2.0)); // avoid log(0) and log(1)
    double const total_timespan(cycle().get_cycle_started_on() - cycle().get_item_oldest_access());
    double const recency(total_timespan > 0.0
                        ? (cycle().get_cycle_started_on() - item().get_last_accessed_on()) / total_timespan
                        : 0.0);

    return frequency * cost / log_size - profile().get_age_weight() * recency;
}



} // namespace magpie
// vim: ts=4 sw=4 et
