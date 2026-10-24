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



/////////////////////// EVICTION POLICY VARIABLES ///////////////////////

void eviction_policy::set_size_eviction_trigger_percent(double percent)
{
    f_size_eviction_trigger_percent = percent;
}


double eviction_policy::get_size_eviction_trigger_percent() const
{
    return f_size_eviction_trigger_percent;
}


std::size_t eviction_policy::get_size_eviction_trigger_limit(std::size_t count_limit) const
{
    return static_cast<std::size_t>(floor(count_limit * get_size_eviction_trigger_percent()));
}


void eviction_policy::set_size_eviction_reduction_percent(double percent)
{
    f_size_eviction_reduction_percent = percent;
}


double eviction_policy::get_size_eviction_reduction_percent() const
{
    return f_size_eviction_reduction_percent;
}


std::size_t eviction_policy::get_size_eviction_reduction_limit(std::size_t count_limit) const
{
    return static_cast<std::size_t>(floor(count_limit * get_size_eviction_reduction_percent()));
}


void eviction_policy::set_access_count_weight(double weight)
{
    f_access_count_weight = weight;
}


double eviction_policy::get_access_count_weight() const
{
    return f_access_count_weight;
}


void eviction_policy::set_recency_weight(double weight)
{
    f_recency_weight = weight;
}


double eviction_policy::get_recency_weight() const
{
    return f_recency_weight;
}


void eviction_policy::set_recency_decay_half_time(double decay_half_time)
{
    if(f_recency_decay_half_time <= 0.0)
    {
        throw invalid_parameter("the recency decay half-time cannot be negative or null.");
    }
    f_recency_decay_half_time = decay_half_time;
}


double eviction_policy::get_recency_decay_half_time() const
{
    return f_recency_decay_half_time;
}


void eviction_policy::set_allocated_weight(double weight)
{
    f_allocated_weight = weight;
}


double eviction_policy::get_allocated_weight() const
{
    return f_allocated_weight;
}


void eviction_policy::set_age_weight(double weight)
{
    f_age_weight = weight;
}


double eviction_policy::get_age_weight() const
{
    return f_age_weight;
}


void eviction_policy::set_random_weight(double random)
{
    f_random_weight = random;
}


double eviction_policy::get_random_weight() const
{
    return f_random_weight;
}


void eviction_policy::set_age_decay_half_time(double decay_half_time)
{
    if(f_age_decay_half_time <= 0.0)
    {
        throw invalid_parameter("the age decay half-time cannot be negative or null.");
    }
    f_age_decay_half_time = decay_half_time;
}


double eviction_policy::get_age_decay_half_time() const
{
    return f_age_decay_half_time;
}


void eviction_policy::set_linear_normalization(linear_t linear_normalization)
{
    f_linear_normalization = linear_normalization;
}


linear_t eviction_policy::get_linear_normalization() const
{
    return f_linear_normalization;
}


/////////////////////// CYCLE VARIABLES ///////////////////////

void eviction_policy::set_cycle_started_on(double cycle_started_on)
{
    f_cycle_started_on = cycle_started_on;
}


double eviction_policy::get_cycle_started_on() const
{
    return f_cycle_started_on;
}


void eviction_policy::set_maximum_access_count(double maximum_access_count)
{
    f_maximum_access_count = maximum_access_count;
}


double eviction_policy::get_maximum_access_count() const
{
    return f_maximum_access_count;
}


void eviction_policy::set_maximum_allocated(double maximum_allocated)
{
    f_maximum_allocated = maximum_allocated;
}


double eviction_policy::get_maximum_allocated() const
{
    return f_maximum_allocated;
}


void eviction_policy::set_oldest_item_created_on(double created_on)
{
    f_oldest_item_created_on = created_on;
}


double eviction_policy::get_oldest_item_created_on() const
{
    return f_oldest_item_created_on;
}


void eviction_policy::set_most_recent_item_update(double updated_on)
{
    f_most_recent_item_update = updated_on;
}


double eviction_policy::get_most_recent_item_update() const
{
    return f_most_recent_item_update;
}


void eviction_policy::set_most_recent_item_touch(double touched_on)
{
    f_most_recent_item_touch = touched_on;
}


double eviction_policy::get_most_recent_item_touch() const
{
    return f_most_recent_item_touch;
}


void eviction_policy::set_oldest_item_update(double oldest_update)
{
    f_oldest_item_update = oldest_update;
}


double eviction_policy::get_oldest_item_update() const
{
    return f_oldest_item_update;
}


void eviction_policy::set_oldest_item_touch(double oldest_touch)
{
    f_oldest_item_touch = oldest_touch;
}


double eviction_policy::get_oldest_item_touch() const
{
    return f_oldest_item_touch;
}


void eviction_policy::set_highest_item_turn(double turn)
{
    f_highest_item_turn = turn;
}


double eviction_policy::get_highest_item_turn() const
{
    return f_highest_item_turn;
}



/////////////////////// ITEM VARIABLES ///////////////////////

void eviction_policy::set_access_count(double access_count)
{
    f_access_count = access_count;
}


double eviction_policy::get_access_count() const
{
    return f_access_count;
}


void eviction_policy::set_last_updated_on(double last_updated_on)
{
    f_last_updated_on = last_updated_on;
}


double eviction_policy::get_last_updated_on() const
{
    return f_last_updated_on;
}


void eviction_policy::set_last_touched_on(double last_touched_on)
{
    f_last_touched_on = last_touched_on;
}


double eviction_policy::get_last_touched_on() const
{
    return f_last_touched_on;
}


void eviction_policy::set_allocated(double allocated)
{
    f_allocated = allocated;
}


double eviction_policy::get_allocated() const
{
    return f_allocated;
}


void eviction_policy::set_item_created_on(double created_on)
{
    f_item_created_on = created_on;
}


double eviction_policy::get_item_created_on() const
{
    return f_item_created_on;
}


void eviction_policy::set_item_timeout(double timeout)
{
    f_item_timeout = timeout;
}


double eviction_policy::get_item_timeout() const
{
    return f_item_timeout;
}


void eviction_policy::set_item_turn(double turn)
{
    f_item_turn = turn;
}


double eviction_policy::get_item_turn() const
{
    return f_item_turn;
}



/////////////////////// NORMALIZATION ///////////////////////

double eviction_policy::normalized_accesses(double default_value) const
{
    if((f_linear_normalization & LINEAR_ACCESSES) != 0)
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
    if(snapdev::quiet_floating_point_equal(f_maximum_access_count, 0.0))
    {
        return default_value;
    }
    return f_access_count / f_maximum_access_count;
}


double eviction_policy::normalized_log_saturate_accesses() const
{
    return log(f_access_count + 1.0) / log(f_maximum_access_count + 1.0);
}


double eviction_policy::normalized_recency(double default_value) const
{
    if((f_linear_normalization & LINEAR_RECENCY) != 0)
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
    double const access_age(f_most_recent_item_update - f_oldest_item_update);
    if(snapdev::quiet_floating_point_equal(access_age, 0.0))
    {
        return default_value;
    }
    return (f_last_touched_on - f_oldest_item_update) / access_age;
}


double eviction_policy::normalized_exp_decay_recency() const
{
    double const delta_time(f_cycle_started_on - f_last_touched_on);
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / f_recency_decay_half_time);
}


double eviction_policy::normalized_allocated(double default_value) const
{
    if((f_linear_normalization & LINEAR_BYTE_SIZE) != 0)
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
 * local storage (SSD or HHD) when memory is a high cost.
 *
 * \return The normalized linear value of the allocated byte size.
 */
double eviction_policy::normalized_linear_allocated(double default_value) const
{
    if(snapdev::quiet_floating_point_equal(f_maximum_allocated, 0.0))
    {
        return default_value;
    }
    return f_allocated / f_maximum_allocated;
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
    return log(f_allocated + 1.0) / log(f_maximum_allocated + 1.0);
}


double eviction_policy::normalized_age(double default_value) const
{
    if((f_linear_normalization & LINEAR_AGE) != 0)
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
    double const item_age(f_cycle_started_on - f_oldest_item_created_on);
    if(snapdev::quiet_floating_point_equal(item_age, 0.0))
    {
        return default_value;
    }
    return 1.0 - (f_cycle_started_on - f_item_created_on) / item_age;
}


double eviction_policy::normalized_exp_decay_age() const
{
    double const delta_time(f_cycle_started_on - f_item_created_on);
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / f_age_decay_half_time);
}


double eviction_policy::normalized_survival(double default_value) const
{
    if((f_linear_normalization & LINEAR_AGE) != 0)
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
    double const item_age(f_cycle_started_on - f_oldest_item_created_on);
    if(snapdev::quiet_floating_point_equal(item_age, 0.0))
    {
        return default_value;
    }
    return (f_item_timeout - f_cycle_started_on) / item_age;
}


double eviction_policy::normalized_exp_decay_survival() const
{
    double const delta_time(f_item_timeout - f_cycle_started_on);
    if(delta_time <= 0.0)
    {
        return 1.0;
    }
    return pow(M_E, -delta_time / f_age_decay_half_time);
}


double eviction_policy::normalized_random() const
{
    return snapdev::random(0.0, 1.0);
}



/////////////////////// COMPUTE WEIGHT IMPLEMENTATIONS ///////////////////////

double eviction_policy_add::compute_weight() const
{
    double weight(0.0);
    if(snapdev::quiet_floating_point_not_equal(f_access_count_weight, 0.0))
    {
        weight += f_access_count_weight * normalized_accesses();
    }
    if(snapdev::quiet_floating_point_not_equal(f_recency_weight, 0.0))
    {
        weight += f_recency_weight * normalized_recency();
    }
    if(snapdev::quiet_floating_point_not_equal(f_allocated_weight, 0.0))
    {
        weight += f_allocated_weight * normalized_allocated();
    }
    if(snapdev::quiet_floating_point_not_equal(f_age_weight, 0.0))
    {
        weight += f_age_weight * normalized_age();
    }
    if(snapdev::quiet_floating_point_not_equal(f_random_weight, 0.0))
    {
        weight += f_random_weight * normalized_random();
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
    return pow(normalized_accesses(),  f_access_count_weight)
         * pow(normalized_recency(),   f_recency_weight)
         * pow(normalized_allocated(), f_allocated_weight)
         * pow(normalized_age(),       f_age_weight);
}


double eviction_policy_harmonic_mean::compute_weight() const
{
    return (f_access_count_weight + f_recency_weight + f_allocated_weight + f_age_weight)
        / (f_access_count_weight / normalized_accesses(1.0)
            + f_recency_weight / normalized_recency(1.0)
            + f_allocated_weight / normalized_allocated(1.0)
            + f_age_weight / normalized_age(1.0));
}


double eviction_policy_cost_benefit_ratio::compute_weight() const
{
    return (f_access_count_weight * normalized_accesses()
                + f_recency_weight * normalized_recency())
                     / pow(f_allocated, f_allocated_weight);
}



} // namespace magpie
// vim: ts=4 sw=4 et
