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
 * \brief The main() function of the magpie library tests.
 *
 * This file implements the main() function of the unit tests used to
 * verify the magpie library.
 *
 * It defines any globals (none at this point) and a few basic command
 * line options such as --help and --version.
 */

// self
//
//#include    "cache_impl.h"
//#include    "cache_ref.h"



// cppthread
//
//#include    <cppthread/guard.h>


// snapdev
//
//#include    <snapdev/not_used.h>


// C++
//
#include    <cstdint>



namespace magpie
{



class cache_interface
{
public:
    virtual             ~cache_interface() = default;

    virtual void        copy(void const * in, void * out, std::size_t size) const = 0;
    virtual void        destroy(void * value, std::size_t size) const = 0;
    virtual std::size_t allocated(void const * value, std::size_t size) const = 0;
};



} // namespace magpie
// vim: ts=4 sw=4 et
