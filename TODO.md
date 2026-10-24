
* Consider using `uint32_t` for all the values in `item_t`.
  - the accesses counter should not go over, that's still 4 billion
  - the size & allocated still allow for objects of 4Gb
    + to avoid issues, we can use the existing maximum object size
  - the `time_t` can be biased with `g_start_time`
  - the TTL really doesn't need to be 64 bits

* Look at fixing up how we compute the limits
  - the `cache_impl` class makes direct use of the different values
  - the `eviction_policy` is optional so some of the values are in `options`

* Other eviction algorithms
  - FIFO -- the first item added gets removed first (i.e. purge oldest
    using the `f_created_on` field)
  - Random -- just delete some items... pick your number and delete it
  - MRU -- Most Recently User ("opposite" of LRU)
  - Secondary Cache -- have some items serialized on disk if really slow
    to gather them otherwise (i.e. an HTTP GET over a slow connection)
  - Remote or Two-Tier Cache -- cache on a separate server; this would not be
    implemented here and could instead use Prinbee which does that already
  - SLRU -- the Segmented Least Recently Used is an LRU where items need to
    win a seat to not be removed early (i.e. if added then forgotten, then
    that item is removed first)


