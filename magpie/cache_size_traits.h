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
 * \brief The trait templates used to get the byte size of objects.
 *
 * This file implements various templates that can be used to determine
 * an estimate of the number of bytes used by an object. This does not
 * do a deep sizing here, however, if your objects do a deep copy from
 * their copy operator, then you can create a trait for your object and
 * return the estimated size of the resulting deep copy.
 */

// self
//
//#include    "cache_impl.h"



// snapdev
//
#include    <snapdev/not_used.h>


// C++
//
#include    <memory>
#include    <set>
#include    <string>
#include    <type_traits>
#include    <vector>



namespace magpie
{



/** \brief Base type returning sizeof(T).
 *
 * This works for all basic types and any structure composed of basic types.
 * However, if you have objects composed of other objects, you may want
 * to declare your own type to properly capture the size of the data
 * getting cached.
 *
 * For example, a structure with a few strings and a buffer of bytes
 * defines this way:
 *
 * \code
 * struct my_data_t
 * {
 *     std::string                 f_name = std::string();
 *     std::string                 f_description = std::string();
 *     int                         f_version = 0;
 *     std::vector<std::uint8_t>   f_buffer = std::vector<std::uint8_t>();
 * };
 * \endcode
 *
 * then the `sizeof(my_data_t)` will return a very small number compared
 * to the real size of that data. A better way is to define your own
 * template and include it before you include the cache.h header:
 *
 * \code
 * namespace magpie
 * {
 *
 * template<>
 * struct cache_size_traits<my_data_t>
 * {
 *     static std::size_t estimate(my_data_t const & d)
 *     {
 *         std::size_t total_size(0);
 *         total_size += magpie::cache_size_traits<decltype(d.f_name)>::estimate(d.f_name);
 *         total_size += magpie::cache_size_traits<decltype(d.f_description)>::estimate(d.f_description);
 *         total_size += sizeof(d.f_version);
 *         total_size += magpie::cache_size_traits<decltype(d.f_buffer)>::estimate(d.f_buffer);
 * 
 *         return total_size;
 *     }
 * };
 *
 * } // namespace magpie
 * \endcode
 *
 * \tparam T  The type being sized.
 * \tparam void  Specialization for objects without a byte_size() function.
 */
template <typename T, typename = void>
struct cache_size_traits
{
    static std::size_t estimate(T const & obj)
    {
        snapdev::NOT_USED(obj);

        return sizeof(T);
    }
};


/** \brief Get the cache size of objects with a byte_size() function.
 *
 * If no specialization is found, try calling the byte_size() function
 * if defined. This is a non-conventional function name (as in, it is
 * not part of the standard library). That means it should not clash
 * with anything else.
 *
 * \tparam T  The type being sized.
 */
template <typename T>
struct cache_size_traits<T, std::void_t<decltype(std::declval<T>().byte_size())>>
{
    static std::size_t estimate(T const & obj)
    {
        return sizeof(T) + obj.capacity(); 
    }
};


template<
    class Key,
    class Compare,
    class Allocator
>
struct cache_size_traits<std::set<Key, Compare, Allocator>>
{
    static std::size_t estimate(std::set<Key, Compare, Allocator> const & s)
    {
        constexpr std::size_t NODE_SIZE = 32;

        std::size_t total_size(sizeof(s) + s.size() * NODE_SIZE);

        if constexpr (std::is_trivially_copyable_v<Key>)
        {
            total_size += s.size() * sizeof(Key);
        }
        else
        {
            for(auto const & e : s)
            {
                total_size += cache_size_traits<Key>::estimate(e);
            }
        }

        return total_size;
    }
};



/** \brief Specialization for std::string of any type.
 *
 * This specialization computes the number of bytes used by a standard
 * string. It uses capacity() instead of size() or length() since the
 * capacity is the buffer currently allocated, opposed to the current
 * length of the string.
 *
 * \tparam CharT  The type of characters in that string.
 * \tparam Traits  The traits of that character type.
 * \tparam Allocator  The string buffer allocator.
 */
template <
    typename CharT,
    typename Traits,
    typename Allocator
>
struct cache_size_traits<std::basic_string<CharT, Traits, Allocator>>
{
    static std::size_t estimate(std::basic_string<CharT, Traits, Allocator> const & s)
    {
        // the GNU std::string has a buffer of 16 bytes used for short strings
        //
        std::size_t const local_capacity(15 / sizeof(CharT));
        if(s.capacity() <= local_capacity)
        {
            return sizeof(s);
        }
        return sizeof(s) + s.capacity() * sizeof(CharT);
    }
};


template<
    class T,
    class Allocator
>
struct cache_size_traits<std::vector<T, Allocator>>
{
    static std::size_t estimate(std::vector<T, Allocator> const & v)
    {
        std::size_t total_size(sizeof(v));

        if constexpr (std::is_trivially_copyable_v<T>)
        {
            total_size += v.capacity() * sizeof(T);
        }
        else
        {
            for(auto const & e : v)
            {
                total_size += cache_size_traits<T>::estimate(e);
            }

            if(v.capacity() > v.size())
            {
                total_size += (v.capacity() - v.size()) * sizeof(T);
            }
        }

        return total_size;
    }
};



} // namespace magpie
// vim: ts=4 sw=4 et
