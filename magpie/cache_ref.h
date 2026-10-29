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
#include    <magpie/exception.h>
#include    <magpie/constraint.h>



// snapdev
//
//#include    <snapdev/not_used.h>


// C++
//
#include    <optional>



namespace magpie
{



template<typename K, typename T>
requires std::copy_constructible<T>
      && (std::is_trivially_copyable_v<K>
            || (has_data<K> && has_size<K>))
class cache;


template<typename K, typename T>
class cache_ref
{
public:
    typedef cache<K, T>     cache_t;

    cache_ref(cache_t * c, K const & k)
        : f_cache(c)
        , f_key(k)
    {
    }

    cache_ref(cache_ref const & rhs)
        : f_cache(rhs.f_cache)
        , f_key(rhs.f_key)
    {
    }

    cache_ref & operator = (cache_ref const & rhs)
    {
        if(this != &rhs)
        {
            f_cache = rhs.f_cache;
            f_key = rhs.f_key;
        }
        return *this;
    }

    cache_ref & operator = (T const & v)
    {
        f_cache->set(f_key, v);
        return *this;
    }

    operator T () const
    {
        typename cache_t::optional_t v(f_cache->get(f_key));
        if(v.has_value())
        {
            return v.value();
        }
        return T(); // return a default/empty T otherwise
    }

    explicit operator bool () const
    {
        return f_cache->contains(f_key);
    }

    template<typename A>
    cache_ref & operator += (A const & a)
    {
        f_cache->add(f_key, a);
        return *this;
    }

    template<typename A>
    T operator + (A const & a) const
    {
        return operator T () + a;
    }

    template<typename A>
    cache_ref & operator -= (A const & a)
    {
        f_cache->add(f_key, -a);
        return *this;
    }

    template<typename A>
    T operator - (A const & a) const
    {
        return operator T () - a;
    }

    T operator * () const
    {
        std::optional<T> v(f_cache->get(f_key));
        if(!v.has_value())
        {
            throw value_not_found("value not currently set.");
        }
        return v.value();
    }

private:
    cache_t *           f_cache = nullptr;
    K                   f_key = K();
};



} // namespace magpie
// vim: ts=4 sw=4 et
