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
 * \brief Various constraints used by the magpie implementation.
 *
 * The classes use constraints to make sure that the cache is used with
 * objects that will actually work.
 */

// self
//
//#include    "cache_impl.h"
//#include    "cache_ref.h"



// snapdev
//
//#include    <snapdev/not_used.h>


// C++
//
#include    <concepts>



namespace magpie
{



template<typename T>
concept has_capacity = requires(T t)
{
    { t.capacity() } -> std::convertible_to<std::size_t>;
};


//template<typename T>
//concept is_trivially_copyable_or_has_data = std::is_trivially_copyable_v<T> || requires(T t)
//{
//    { t.data() } -> std::same_as<std::conditional_t<true, decltype(t.data()), void>>; 
//    requires std::is_pointer_v<decltype(t.data())>;
//};


template<typename T>
concept has_data = requires(T t)
{
    { t.data() } -> std::same_as<std::conditional_t<true, decltype(t.data()), void>>; 
    requires std::is_pointer_v<decltype(t.data())>;
};


//template<typename T>
//concept is_trivially_copyable_or_has_size = std::is_trivially_copyable_v<T> || requires(T t)
//{
//    { t.size() } -> std::convertible_to<std::size_t>;
//};


template<typename T>
concept has_size = requires(T t)
{
    { t.size() } -> std::convertible_to<std::size_t>;
};



} // namespace magpie
// vim: ts=4 sw=4 et
