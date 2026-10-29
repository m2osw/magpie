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
 * \brief Utility functions and variables.
 *
 * The mapgpie cache uses these utility functions and variables in
 * various places.
 *
 * These are useful global functions.
 */

// self
//
#include    <magpie/utils.h>



// last include
//
#include    <snapdev/poison.h>



namespace magpie
{
namespace
{



/** \brief Time at which the application started.
 *
 * This variable holds the time when your application loaded the
 * magpie .so dynamic library.
 *
 * \note
 * We do a `time(nullptr) - 1` because the special time 0 represents
 * 0 in time_t and biased_time_t. So if you very quickly create an
 * item in a cache, you could end up with an overload.
 */
time_t const                    g_start_time = time(nullptr) - 1;



} // no name namespace



time_t magpie_start_time()
{
    return g_start_time;
}


/** \brief Transform 'now' in a 32 bit unsigned integer.
 *
 * This function returns 'now' in a 32 bit unsigned integer. It does
 * that by subtracting g_start_time (see magpie_start_time() for details)
 * from time(nullptr). As a result we end up with a time which is small
 * enough to find in an unsigned integer of 32 bits and still last
 * well over 100 years.
 *
 * \note
 * A very early call means the function may return 0.
 *
 * \return The biased_time_t representing now() in seconds.
 */
biased_time_t biased_now()
{
    return static_cast<biased_time_t>(time(nullptr) - g_start_time);
}


time_t unbias_biased_time(biased_time_t biased_time)
{
    if(biased_time == 0)
    {
        return time_t();
    }
    return static_cast<time_t>(biased_time) + g_start_time;
}



} // namespace magpie
// vim: ts=4 sw=4 et
