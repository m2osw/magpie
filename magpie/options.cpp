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
 * \brief The cache setup is done using these options.
 *
 * The mapgpie::cache class must be initialized in the constructor
 * because once initialized, changes to the settings are not permitted.
 * For that reason, it uses an options class to pre-define all the
 * settings.
 */

// self
//
#include    <magpie/options.h>

#include    <magpie/exception.h>



// snapdev
//
//#include    <snapdev/not_used.h>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{



void options::set_least_recently_used(bool const & lru)
{
    f_lru = lru;
}


bool options::get_least_recently_used() const
{
    return f_lru;
}


void options::set_least_frequently_used(bool const & lfu)
{
    f_lfu = lfu;
}


bool options::get_least_frequently_used() const
{
    return f_lfu;
}


void options::set_allow_empty_key(bool allowed)
{
    f_allow_empty_key = allowed;
}


bool options::get_allow_empty_key() const
{
    return f_allow_empty_key;
}


void options::set_capacity(std::size_t maximum_byte_size)
{
    f_maximum_byte_size = maximum_byte_size;
}


std::size_t options::get_capacity() const
{
    return f_maximum_byte_size;
}


void options::set_count(std::size_t maximum_count)
{
    f_maximum_count = maximum_count;
}


std::size_t options::get_count() const
{
    return f_maximum_count;
}


void options::set_default_ttl(duration_t ttl)
{
    if(ttl != magpie::NO_TTL
    && ttl != magpie::DEFAULT_TTL
    && ttl <= 0)
    {
        throw out_of_range("options::set_default_ttl(): ttl must be a positive number, magpie::NO_TTL, or magpie::DEFAULT_TTL.");
    }

    f_default_ttl = ttl;
}


duration_t options::get_default_ttl() const
{
    return f_default_ttl;
}


void options::set_maximum_lifetime(duration_t max)
{
    if(max < NO_MAXIMUM_LIFETIME)
    {
        throw out_of_range("options::set_max_lifetime(): the maximum lifetime parameter must be positive or 0 (NO_MAXIMUM_LIFETIME).");
    }

    f_maximum_lifetime = max;
}


duration_t options::get_maximum_lifetime() const
{
    return f_maximum_lifetime;
}


void options::set_cleanup_interval(duration_t interval)
{
    f_cleanup_interval = interval;
}


duration_t options::get_cleanup_interval() const
{
    return f_cleanup_interval;
}


void options::set_lock_model(lock_model_t model)
{
    f_lock_model = model;
}


lock_model_t options::get_lock_model() const
{
    return f_lock_model;
}


void options::set_wait_lock_for_stale_data_deletion(timespec const & max_wait)
{
    f_wait_lock_for_stale_data_deletion = max_wait;
}


timespec const & options::get_wait_lock_for_stale_data_deletion() const
{
    return f_wait_lock_for_stale_data_deletion;
}


void options::set_stale_data(stale_data_t mode)
{
    f_stale_data = mode;
}


stale_data_t options::get_stale_data() const
{
    return f_stale_data;
}


void options::set_bucket_count(std::size_t count)
{
    // make sure the user doesn't use a completely invalid number here
    //
    if(count < 1
    || count > MAX_BUCKET_COUNT)
    {
        throw out_of_range("bucket count out of range.");
    }

    if(count % EVICT_SWEEP_STRIDE == 0)
    {
        // you need the EVICT_SWEEP_STRIDE to be a coprime of f_bucket_count
        // which generally means a prime larger than the maximum
        // f_bucket_count to make sure it work with any number
        //
        throw logic_error("bucket count is a multiple of EVICT_SWEEP_STRIDE."); // LCOV_EXCL_LINE
    }

    f_bucket_count = count;
}


std::size_t options::get_bucket_count() const
{
    return f_bucket_count;
}



} // namespace magpie
// vim: ts=4 sw=4 et
