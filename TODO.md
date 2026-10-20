
* Consider using `uint32_t` for all the values in `item_t`.
  - the accesses counter should not go over, that's still 4 billion
  - the size & allocated still allow for objects of 4Gb
    + to avoid issues, we can use the existing maximum object size
  - the `time_t` can be biased with `g_start_time`
  - the TTL really doesn't need to be 64 bits

