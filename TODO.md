
* Consider using `uint32_t`/`unt16_t` for all the values in `item_t`.
  - the size & allocated still allow for objects of 4Gb
    + to avoid issues, we can use the existing maximum object size

* Look at fixing up how we compute the allocated & limits
  - the `cache_impl` class makes direct use of the different values
  - the `eviction_policy` is optional so some of the values are in `options`

* Add callbacks for various features
  - delete an item signal
  - reached a limit (capacity / # of items)

* Other eviction algorithms
  - Secondary Cache -- have some items serialized on disk if really slow
    to gather them otherwise (i.e. an HTTP GET over a slow connection)
  - Remote or Two-Tier Cache -- cache on a separate server; this would not be
    implemented here and could instead use Prinbee which does that already

* Consider breaking the `eviction_policy.h/cpp` in separate files
  - the actual `eviction_policy` class can stay in the same files
  - move the profile, cycle, and item to separate files
  - maybe create a separate file for the implementations of the policy

* Replace `f_field = 123` with `f_field{123}` syntax to possibly avoid errors.
  - Note: this is true for all of our code... lots of work ahead :)

* The cache manager adds/removes caches to/from a vector and we walk that
  vector to evict items that are old, large, recently touched, etc. that
  works well when adding, but when removing, we may end up skipping a
  cache eviction process in the current cycle. Without managing caches
  dynamically (i.e. creating/deleting caches all the time instead of
  creating them at initialization) makes it very less important at the
  moment.

