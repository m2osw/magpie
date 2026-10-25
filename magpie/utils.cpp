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
#include    <magpie/utils.h>



// snapdev
//
//#include    <snapdev/not_used.h>


// last include
//
#include    <snapdev/poison.h>



namespace magpie
{
namespace
{



time_t const                    g_start_time = time(nullptr);



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
    return static_cast<time_t>(biased_time) + g_start_time;
}



} // namespace magpie
// vim: ts=4 sw=4 et
