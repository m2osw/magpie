
<p align="center">
<img alt="magpie" title="Memory cache as a strongly typed C++ template as the user interface."
src="https://github.com/m2osw/magpie/blob/main/doc/magpie.png" width="482" height="294"/>
</p>

# Magpie

Magpie is a C++ library handling an advanced memory cache.

It supports TTL, LRU, LFU, Capacity, Count, and a few other features.

## Limitations

The library accepts any type of object, including complex C structures.
It makes a copy of the objects in the cache on a `set()` and then a
copy from the cache on a `get()`. So the cache itself cannot be changed
if the object is a simple object such as a basic type (`int`, `double`,
etc.) or a self-contained object (such as an `std::string`).

Obviously, any object that cannot be copied cannot be added to the
cache unless you pass a pointer to that object (whether it is a
smart pointer or a bare pointer). In that case, the object is only
partially handled in the cache. Especially, if you just pass a pointer
(i.e. the template type `T` is set to `my_object *`) then the object
itself doesn't get copied at all. That means you need means to keep
the object alive outside of the cache, which probably defies having
a cache in the first place.

Using a shared pointer resolves the issue of still tracking the
object outside of the cache. However, it does not help with the
capacity issue where by default the estimated size returns the
size of the shared pointer. You are responsible for creating a
trait if you want to get a valid capacity. If your objects have
a pretty constant size, then you can use the count instead
since that gives you a way to determine the amount of memory
used (i.e. average size X count = total allocated bytes).

Still to make it save, it is advice that you only save objects that
self copy properly so you can save a copy in the cache, retrieve
a copy of the object in the cache, and update that copy using
the `set()` function.

For buffers of data, you can always use an `std::vector<std::uint8_t>`
or an `std::string` which work just fine out of the box.

## Least Recently Used (LRU)

The library tracks the **Least Recently Used** cached objects and remove
those first.

By default, the library extends the time an item will live on each
call to get(). That can be turned off so the very first TTL defines
the exact time when the data is considered stale.

## Least Frequently Used (LFU)

The library tracks the **Least Frequently Used** cached object using a
counter that gets incremented on each call to the get() function.

## Capacity

The library tracks how much memory is used (with help from your objects
which need to implement a capacity() function). If that limit is reached,
the eviction code starts working on which items to remove.

### Cache Size Traits

The library offers a `magpie::cache_size_traits` set of templates for
various standard types. If your object cannot be sized using the
existing templates, you may need to create additional traits or implement
the `byte_size()` function on your object.

The `cache::allocated()` function returns the total amount of estimated
memory currently used by the items in the cache. It does not include
the amount of memory used by the cache structures.

## Count

The library knows how many objects are cached. If it reaches a specified
number then older items get removed by LRU or LFU.

## Murmur3 Keys

The library automatically transforms keys to a murmur3 sum. For
objects with very long keys (i.e. long URIs), it saves a lot of
memory. Also the hash distribution helps memory management and
with speed. This is because internally the library uses separate
buckets to avoid locking threads for long period of times.
The library may lock bucket 3 for a `set()` and bucket 5 once
& 1 three times for a total of four `get()`s from other threads.
All of those can run in parallel along the background cleanup
thread which may now be working on bucket 4.

## Callbacks

You may setup various callbacks to receive signals whenever something
happens (i.e. an object is removed from the cache, the capacity was
reached, etc.)

## Conditional Add

Based on various parameters, trying to add an item to the cache may
_fail_. That is, the library may decide that the object is probably not
worth keeping for any amount of time and never caches it.



# License

This library is covered by the GPL 3.0.

The project includes some Unicode files which are covered by the Unicode
license.


# Bugs

Submit bug reports and patches on
[github](https://github.com/m2osw/magpie/issues).


_This file is part of the [snapcpp project](https://snapwebsites.org/)._
