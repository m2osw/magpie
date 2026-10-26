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
#include    "cache_impl.h"
#include    "cache_ref.h"
#include    "cache_size_traits.h"
#include    "lock_guard.h"



// cppthread
//
#include    <cppthread/guard.h>


// snapdev
//
//#include    <snapdev/not_used.h>


// C++
//
#include    <iostream>
#include    <memory>



namespace magpie
{



struct empty_t
{
};


template<typename K, typename T>
requires std::copy_constructible<T>
      && (std::is_trivially_copyable_v<K>
            || (has_data<K> && has_size<K>))
class cache
    : public cache_interface
{
public:
    typedef std::shared_ptr<cache>      pointer_t;

    cache(options const & opts)
        : f_impl(std::make_shared<detail::cache_impl>(this, opts))
    {
        f_allow_empty_key = opts.get_allow_empty_key();

        // the following CANNOT be placed in the header because the
        // shared pointer is necessary before the start_cleanup_worker()
        // can be called
        //
        if(f_impl->is_cleanup_worker_required())
        {
            f_impl->start_cleanup_worker();
        }
    }

    ~cache()
    {
        f_impl->cache_done();
        f_impl.reset();

    }

    constexpr std::size_t sizeof_T() const
    {
        if constexpr (std::is_same_v<T, empty_t>)
        {
            return std::size_t();
        }
        else
        {
            return sizeof(T);
        }
    }

    set_result_t set(K const & k, T const & t, duration_t ttl = DEFAULT_TTL, float cost = NO_COST)
    {
        if(!f_allow_empty_key
        && key_size(k) == 0)
        {
            throw empty_key_not_accepted("this cache was setup to not accept the empty key.");
        }

        return f_impl->set(hash_key(k), &t, sizeof_T(), ttl, cost);
    }

    bool erase(K const & k)
    {
        return f_impl->erase(hash_key(k));
    }

    bool ban(K const & k)
    {
        return f_impl->ban(hash_key(k));
    }

    void unban(K const & k)
    {
        f_impl->unban(hash_key(k));
    }

    [[nodiscard]] bool contains(K const & k) const
    {
        return f_impl->contains(hash_key(k));
    }

    [[nodiscard]] std::optional<T> get(K const & k, bool update_ttl = false) const
    {
        return get_impl(hash_key(k), update_ttl);
    }

    std::optional<T> operator [] (K const & k) const
    {
        return get_impl(hash_key(k));
    }

    cache_ref<K, T> operator [] (K const & k)
    {
        return cache_ref<K, T>(this, hash_key(k));
    }

    [[nodiscard]] metadata_t get_metadata(K const & k) const
    {
        return f_impl->get_metadata(k);
    }

    template<typename A>
    requires requires(T t, A a) { { t + a } -> std::convertible_to<T>; }
    T add(K const & k, T const & a)
    {
        detail::hash_t const h(hash_key(k));
        lock_interface::pointer_t lock(f_impl->get_lock(h));
        if(!lock->supports_recursivity())
        {
            throw incompatible_lock(
                  std::string("the add(), inc(), dec() are not available with lock model \"")
                + lock->get_name()
                + "\".");
        }
        lock_guard guard(lock);
        std::optional<T> v(get_impl(h, true, false));
        if(v.has_value())
        {
            T result(v.value() + a);
            set(h, result);
            return result;
        }
        else
        {
            set(h, a);
            return T(a);
        }
    }

    T inc(K const & k)
    {
        return add(k, 1);
    }

    T dec(K const & k)
    {
        return add(k, -1);
    }

    virtual void copy(void const * in, void * out, std::size_t size) const override
    {
        if(size != sizeof_T())
        {
            throw logic_error("size of T and size of object to be copied do not match."); // LCOV_EXCL_LINE
        }

//std::cerr << gettid() << ": --- COPY AN OBJECT " << reinterpret_cast<void const *>(this) << " :: " << in << " -> " << out << " ---" << std::endl;
        if(size > 0)
        {
            std::construct_at(reinterpret_cast<T *>(out), *reinterpret_cast<T const *>(in));
        }
    }

    virtual void destroy(void * value, std::size_t size) const override
    {
        if(size != sizeof_T())
        {
            throw logic_error("size of T and size of object to be destroyed do no match."); // LCOV_EXCL_LINE
        }
//std::cerr << gettid() << ": --- DESTROY AN OBJECT " << reinterpret_cast<void const *>(this) << " :: " << value << " ---" << std::endl;
        if(size > 0)
        {
            T * v(reinterpret_cast<T *>(value));
            std::destroy_at(v);
        }
    }

    std::size_t size() const
    {
        return f_impl->size();
    }

    std::size_t allocated() const
    {
        return f_impl->allocated();
    }

#ifdef _DEBUG
    void verify_current_count()
    {
        f_impl->verify_current_count();
    }

    void verify_current_capacity()
    {
        f_impl->verify_current_capacity();
    }
#endif

    virtual std::size_t allocated(void const * value, std::size_t size) const override
    {
        if(size != sizeof_T())
        {
            throw logic_error("size of T and size of object in allocated() do no match."); // LCOV_EXCL_LINE
        }

        T const * v(reinterpret_cast<T const *>(value));
        return cache_size_traits<T>::estimate(*v);
    }

private:
    //friend detail::cache_impl; -- unfortunately, we need to have that interface so it's not useful to hide the functions...

    std::size_t key_size(K const & k) const
    {
        if constexpr (has_data<K> && has_size<K>)
        {
            return k.size();
        }
        else
        {
            return sizeof(K);
        }
    }

    detail::hash_t hash_key(K const & k) const
    {
        if constexpr (has_data<K> && has_size<K>)
        {
            return f_impl->hash_key(k.data(), k.size());
        }
        else
        {
            return f_impl->hash_key(&k, sizeof(K));
        }
    }

    std::optional<T> get_impl(detail::hash_t const h, bool update_ttl = true) const
    {
        T v;
        if(f_impl->get(h, &v, sizeof_T(), update_ttl))
        {
            return std::optional<T>(std::move(v));
        }

        return std::nullopt;
    }

    detail::cache_impl::pointer_t   f_impl = detail::cache_impl::pointer_t();
    bool                            f_allow_empty_key = false;
};



} // namespace magpie
// vim: ts=4 sw=4 et
