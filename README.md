# Magpie

Magpie is a C++ library handling an advanced memory cache.

It supports LRU, LFU, Capacity, Count, and a few other features.

## LRU

The library tracks the Least Recently Used cached object and remove those
first.

The library supports a setup where the last GET updates the last access date
and another where a GET does not update the last access date.

## LFU

The library tracks the Least Frequently Used cached object using a counter
that gets incremented on each GET.

## Capacity

The library tracks how much memory it is used (with help from your objects
which need to implement a capacity() function). If the limit is reached,
the one of the algorithm (LRU, LFU) is used to remove some of the items.

## Count

The library knows how many objects are cached. If it reaches a specified
number then older items get removed by LRU or LFU before new ones get added.

## MD5 Keys

The library can automatically transform your keys to an MD5 sum. For
objects with very long keys (i.e. long URIs), it can save a lot of
memory. Also the distribution will help memory management.

## Callbacks

You may setup various callbacks to receive signals whenever something
happens (i.e. an object is removed from the cache, the capacity was
reached, etc.)

## Conditional Add

Based on various parameters, trying to add a parameter to the cache may
_fail_. That is, the library may decide that the object is probably not
worth keeping for any amount of time and never caches it.




