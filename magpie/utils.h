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
 * \brief The cache helper functions and types.
 *
 * The cache makes use of some common functions and types that are defined
 * in the utils header and implementation.
 */

// self
//
//#include    <magpie/eviction_policy.h>



// C++
//
#include    <cstdint>


// C
//
#include    <time.h>



namespace magpie
{



typedef std::uint32_t                   biased_time_t;


time_t          magpie_start_time();
biased_time_t   biased_now();



} // namespace magpie
// vim: ts=4 sw=4 et
