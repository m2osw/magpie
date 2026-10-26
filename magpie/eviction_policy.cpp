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



/////////////////////// PROFILE VARIABLES ///////////////////////




#if 0
void eviction_policy::set_size_eviction_trigger_percent(double percent)
{
    f_profile.f_size_eviction_trigger_percent = percent;
}


double eviction_policy::get_size_eviction_trigger_percent() const
{
    return f_profile.f_size_eviction_trigger_percent;
}


void eviction_policy::set_size_eviction_reduction_percent(double percent)
{
    f_profile.f_size_eviction_reduction_percent = percent;
}


double eviction_policy::get_size_eviction_reduction_percent() const
{
    return f_profile.f_size_eviction_reduction_percent;
}


void eviction_policy::set_access_count_weight(double weight)
{
    f_profile.f_access_count_weight = weight;
}


double eviction_policy::get_access_count_weight() const
{
    return f_profile.f_access_count_weight;
}


void eviction_policy::set_recency_weight(double weight)
{
    f_profile.f_recency_weight = weight;
}


double eviction_policy::get_recency_weight() const
{
    return f_profile.f_recency_weight;
}


void eviction_policy::set_recency_decay_half_time(double decay_half_time)
{
    if(f_profile.f_recency_decay_half_time <= 0.0)
    {
        throw invalid_parameter("the recency decay half-time cannot be negative or null.");
    }
    f_profile.f_recency_decay_half_time = decay_half_time;
}


double eviction_policy::get_recency_decay_half_time() const
{
    return f_profile.f_recency_decay_half_time;
}


void eviction_policy::set_allocated_weight(double weight)
{
    f_profile.f_allocated_weight = weight;
}


double eviction_policy::get_allocated_weight() const
{
    return f_profile.f_allocated_weight;
}


void eviction_policy::set_age_weight(double weight)
{
    f_profile.f_age_weight = weight;
}


double eviction_policy::get_age_weight() const
{
    return f_profile.f_age_weight;
}


void eviction_policy::set_age_decay_half_time(double decay_half_time)
{
    if(f_profile.f_age_decay_half_time <= 0.0)
    {
        throw invalid_parameter("the age decay half-time cannot be negative or null.");
    }
    f_profile.f_age_decay_half_time = decay_half_time;
}


double eviction_policy::get_age_decay_half_time() const
{
    return f_profile.f_age_decay_half_time;
}


void eviction_policy::set_survival_weight(double weight)
{
    f_profile.f_survival_weight = weight;
}


double eviction_policy::get_survival_weight() const
{
    return f_profile.f_survival_weight;
}


void eviction_policy::set_survival_decay_half_time(double decay_half_time)
{
    if(f_profile.f_survival_decay_half_time <= 0.0)
    {
        throw invalid_parameter("the survival decay half-time cannot be negative or null.");
    }
    f_profile.f_survival_decay_half_time = decay_half_time;
}


double eviction_policy::get_survival_decay_half_time() const
{
    return f_profile.f_survival_decay_half_time;
}


void eviction_policy::set_random_weight(double random)
{
    f_profile.f_random_weight = random;
}


double eviction_policy::get_random_weight() const
{
    return f_profile.f_random_weight;
}


void eviction_policy::set_linear_normalization(linear_t linear_normalization)
{
    f_profile.f_linear_normalization = linear_normalization;
}


linear_t eviction_policy::get_linear_normalization() const
{
    return f_profile.f_linear_normalization;
}
#endif


/////////////////////// CYCLE VARIABLES ///////////////////////

void eviction_cycle::new_cycle(
      biased_time_t const & first_item_created_on
    , biased_time_t const & last_item_touched_on)
{
    f_started_on = biased_now();
    ++f_turn;

    f_item_maximum_access_count = 1.0;
    f_item_maximum_allocated = 1.0;
    f_item_oldest_created_on = std::numeric_limits<biased_time_t>::max();
    f_item_most_recent_update = first_item_created_on;
    f_item_most_recent_touch = last_item_touched_on;
    f_item_oldest_update = std::numeric_limits<biased_time_t>::max();
    f_item_oldest_touch = std::numeric_limits<biased_time_t>::max();
    f_item_furthest_timeout = biased_time_t();
    f_item_highest_turn = 0.0;
}





#if 0
void eviction_policy::set_cycle_started_on(double cycle_started_on)
{
    f_cycle.f_started_on = cycle_started_on;
}


double eviction_policy::get_cycle_started_on() const
{
    return f_cycle.f_started_on;
}


void eviction_policy::set_item_maximum_access_count(double maximum_access_count)
{
    f_cycle.f_item_maximum_access_count = maximum_access_count;
}


double eviction_policy::get_item_maximum_access_count() const
{
    return f_cycle.f_item_maximum_access_count;
}


void eviction_policy::set_item_maximum_allocated(double maximum_allocated)
{
    f_cycle.f_item_maximum_allocated = maximum_allocated;
}


double eviction_policy::get_item_maximum_allocated() const
{
    return f_cycle.f_item_maximum_allocated;
}


void eviction_policy::set_item_oldest_created_on(double created_on)
{
    f_cycle.f_item_oldest_created_on = created_on;
}


double eviction_policy::get_item_oldest_created_on() const
{
    return f_cycle.f_item_oldest_created_on;
}


void eviction_policy::set_item_most_recent_update(double updated_on)
{
    f_cycle.f_item_most_recent_update = updated_on;
}


double eviction_policy::get_item_most_recent_update() const
{
    return f_cycle.f_item_most_recent_update;
}


void eviction_policy::set_item_most_recent_touch(double touched_on)
{
    f_cycle.f_item_most_recent_touch = touched_on;
}


double eviction_policy::get_item_most_recent_touch() const
{
    return f_cycle.f_item_most_recent_touch;
}


void eviction_policy::set_item_oldest_update(double oldest_update)
{
    f_cycle.f_item_oldest_update = oldest_update;
}


double eviction_policy::get_item_oldest_update() const
{
    return f_cycle.f_item_oldest_update;
}


void eviction_policy::set_item_oldest_touch(double oldest_touch)
{
    f_cycle.f_item_oldest_touch = oldest_touch;
}


double eviction_policy::get_item_oldest_touch() const
{
    return f_cycle.f_item_oldest_touch;
}


void eviction_policy::set_item_highest_turn(double turn)
{
    f_cycle.f_item_highest_turn = turn;
}


double eviction_policy::get_item_highest_turn() const
{
    return f_cycle.f_item_highest_turn;
}



/////////////////////// ITEM VARIABLES ///////////////////////

void eviction_policy::set_access_count(double access_count)
{
    f_item.f_access_count = access_count;
}


double eviction_policy::get_access_count() const
{
    return f_item.f_access_count;
}


void eviction_policy::set_last_updated_on(double last_updated_on)
{
    f_item.f_last_updated_on = last_updated_on;
}


double eviction_policy::get_last_updated_on() const
{
    return f_item.f_last_updated_on;
}


void eviction_policy::set_last_touched_on(double last_touched_on)
{
    f_item.f_last_touched_on = last_touched_on;
}


double eviction_policy::get_last_touched_on() const
{
    return f_item.f_last_touched_on;
}


void eviction_policy::set_allocated(double allocated)
{
    f_item.f_allocated = allocated;
}


double eviction_policy::get_allocated() const
{
    return f_item.f_allocated;
}


void eviction_policy::set_item_created_on(double created_on)
{
    f_item.f_created_on = created_on;
}


double eviction_policy::get_item_created_on() const
{
    return f_item.f_created_on;
}


void eviction_policy::set_item_timeout(double timeout)
{
    f_item.f_timeout = timeout;
}


double eviction_policy::get_item_timeout() const
{
    return f_item.f_timeout;
}


void eviction_policy::set_item_turn(double turn)
{
    f_item.f_turn = turn;
}


double eviction_policy::get_item_turn() const
{
    return f_item.f_turn;
}
#endif



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
    if(snapdev::quiet_floating_point_equal(cycle().get_item_maximum_access_count(), 0.0))
    {
        return default_value;
    }
    return f_item.get_access_count() / cycle().get_item_maximum_access_count();
}


double eviction_policy::normalized_log_saturate_accesses() const
{
    return log(f_item.get_access_count() + 1.0) / log(cycle().get_item_maximum_access_count() + 1.0);
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
 * local storage (SSD or HHD) when memory is a high cost.
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
std::cout << "access count: " << profile().get_access_count_weight() << " * " << normalized_accesses() << std::endl;
        weight += profile().get_access_count_weight() * normalized_accesses();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_recency_weight(), 0.0))
    {
std::cout << "recency: " << profile().get_recency_weight() << " * " << normalized_recency() << std::endl;
        weight += profile().get_recency_weight() * normalized_recency();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_allocated_weight(), 0.0))
    {
std::cout << "allocated: " << profile().get_allocated_weight() << " * " << normalized_allocated() << std::endl;
        weight += profile().get_allocated_weight() * normalized_allocated();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_age_weight(), 0.0))
    {
std::cout << "age: " << profile().get_age_weight() << " * " << normalized_age() << std::endl;
        weight += profile().get_age_weight() * normalized_age();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_survival_weight(), 0.0))
    {
std::cout << "survival: " << profile().get_survival_weight() << " * " << normalized_survival() << std::endl;
        weight += profile().get_survival_weight() * normalized_survival();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_turn_weight(), 0.0))
    {
std::cout << "turn: " << profile().get_turn_weight() << " * " << normalized_turn() << std::endl;
        weight += profile().get_turn_weight() * normalized_turn();
    }
    if(snapdev::quiet_floating_point_not_equal(profile().get_random_weight(), 0.0))
    {
std::cout << "random: " << profile().get_random_weight() << " * " << normalized_random() << std::endl;
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
    if(item().get_access_count() <= f_minimum_access_count)
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



} // namespace magpie
// vim: ts=4 sw=4 et
