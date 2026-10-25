
# Cache Options

## The Eviction Weight Algorithm

The eviction mechanism first computes a set of items to evict. To do so
it currently uses two equations:

Normalized recency as a 0.0 to 1.0 number:

                           last_used - start_time
    least_recently_used = ------------------------
                              now - start_time

A decaying recency score can also be used so older entries "get older faster":

    least_recently_used = e ** (-recency_weight * delta(t) / t/half-life)

where delta(t) is (now - last access).

Normalized frequency as a 0.0 to 1.0 number:

                             max_frequency - item_frequency
    least_frequently_used = --------------------------------
                                     max_frequency

The frequency can instead use a logarithm. Like the recency, a linear
frequency may not be as useful:

    least_frequently_used = ln(item_frequency + 1)

Then a weight is applied to each normalized value so you can give one of
the normalized value more weight than the other:

    weighted_value = value * weight

Note that the weight can be applied in different ways. The above is the
usual for most algorithms.

Finally, an algorithm is applied to mix the two values together:

| Strategy Flag  |   Logic Type    |           Who does it protect most?            | Vibe / Personality |
|----------------|-----------------|------------------------------------------------|--------------------|
| ADD            | Balanced OR     | Both, but favors extreme highs.                | Fair & steady      |
| MULTIPLY       | Strict AND      | Items that are active right now.               | Fast & ruthless    |
| MINIMUM        | Bottleneck AND  | Strictly penalizes the weakest metric.         | Ultra-pessimistic  |
| MAXIMUM        | Pure OR         | Equal protection for the fresh and the famous. | Ultra-protective   |
| GEOMETRIC MEAN | Aggressive AND  | Balance which item to use.                     | Weighted ruthless  |
| HARMONIC MEAN  | Fair AND        | Gently penalizes the weakest metric.           | Pessimistic        |

The geometric means uses the weights as powers:

    result = recency ** recency_weight * frequency ** frequency_weight

The harmonic mean uses the weights like so:

               recency_weight  +  frequency_weight
    result = ---------------------------------------
               recency_weight     frequency_weight
	      ---------------- + ------------------
	          recency           frequency

To take the byte size of the item in account, you can use it in the
ADD formula like so:

    result = recency_weight * recency + frequency_weight * frequency - size_weight * size

Here `size` is normalized:

                  byte size
    result = -------------------
              maximum byte size

This is because the size has a negative weight on the formula as a whole.

In the geometric approach, you use the inverse instead:

              recency_weight * recency + frequency_weight * frequency
    result = ---------------------------------------------------------
                                 size ** size_weight

### Timestamps

The algorithms have access to many Unix timestamps (`time_t`).

    ---+----+----+----+----+----+----+----+----+----+----... future ...
       |    |    |    |    |    |    |    |    |    |
       |    |    |    |    |    |    |    |    |    +---> [P] item purge time (f_item_timeout)
       |    |    |    |    |    |    |    |    |
       |    |    |    |    |    |    |    |    +---> [E] eviction start time (f_cycle_started_on) -- per cycle
       |    |    |    |    |    |    |    |
       |    |    |    |    |    |    |    +---> [R] last item touch (f_most_recent_item_touch) -- 
       |    |    |    |    |    |    |
       |    |    |    |    |    |    +---> [U] last item update (f_most_recent_item_update) -- one specific item per cycle
       |    |    |    |    |    |
       |    |    |    |    |    +---> [T] item last touched (f_item_last_touched_on)
       |    |    |    |    |
       |    |    |    |    +---> [L] item last updaed (f_item_last_updated_on)
       |    |    |    |
       |    |    |    +---> [O] oldest update (f_item_oldest_update) -- one specific item per cycle
       |    |    |
       |    |    +---> [C] item created on (f_item_created_on)
       |    |
       |    +---> [F] first created item (f_oldest_item_created_on) -- one specific item per cycle
       |
       +---> [S] application start time (magpie_start_time()) -- application wide

    double                  f_cycle_started_on = 0.0;           // timestamp when weight computations started
    double                  f_oldest_item_created_on = 0.0;     // timestamp of when the oldest item was created
    double                  f_most_recent_item_update = 0.0;    // timestamp of the most recent item update
    double                  f_most_recent_item_touch = 0.0;     // timestamp of the most recent item touch
    double                  f_oldest_item_update = 0.0;         // timestamp of the item with the oldest f_last_updated_on
    double                  f_oldest_item_touch = 0.0;          // timestamp of the item with the oldest f_last_touched_on

    double                  f_item_created_on = 0.0;            // timestamp when object was first inserted in cache
    double                  f_item_last_updated_on = 0.0;            // timestamp of last set()
    double                  f_item_last_touched_on = 0.0;            // timestamp of last get()
    double                  f_item_timeout = 0.0;               // timestamp when the item is to timeout

The order is what is most likely. In this drawing, the interval between
each item is kept equal, but it is not the case in the real world. Not
only that, since item specific timestamps can be updated **after** a
new eviction starts, it can actually appear after that time
(i.e. `f_cycle_started_on < f_most_recent_item_update` can be true).

### Setup for LRU mode

The ADD algorithm can be used to setup the eviction mode to LRU.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_recency_weight(1.0);
    opts.set_eviction_policy(ep);

Equation:

$$\frac{T - O}{U - O}$$


### Setup for LFU mode

The ADD algorithm can be used to setup the eviction mode to LRU.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_access_count_weight(1.0);
    opts.set_eviction_policy(ep);

### Setup for LRFU mode

It is possible to combine all the parameters. The most common combination
is the Least Recently/Frequently Used (LRFU) which makes use of the least
recently used and least frequently used values and eliminates those items
first.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_recency_weight(1.0);
    ep->profile().set_access_count_weight(1.0);
    opts.set_eviction_policy(ep);

In most cases, you give the same weight to both. If you want to privilege
one over the other, you can change the weights. For example, to give an
advantage to the more frequently used items, you could use:

    ep->profile().set_recency_weight(0.25);
    ep->profile().set_access_count_weight(0.75);

**Note:** The sum of the weights do not need to be exactly 1.0. Each weight
          is conceptually normalized to a number from 0.0 to 1.0. However,
	  since the eviction is processed on a live cache, it is possible
	  that some of the numbers end up being more than 1.0 or negative.

### Setup for FIFO mode

You can easily tweak the ADD algorithm to make use of the age only and
get a FIFO eviction algorithm.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_age_weight(1.0);
    opts.set_eviction_policy(ep);

This way, the date when the item was created is used one to one. So the
oldest items get removed first. No other parameters are taken in account.

### Setup for Random mode

Although I personally find that to be a strange choice, it is possible
to randomize which items get removed from the cache by selecting them
randomly. The algorithm uses a random number from 0.0 to 1.0 to sort
all the items currently in the cache and then starts evicting items
that got the smallest number.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_random_weight(1.0);
    opts.set_eviction_policy(ep);

### Most Recently Used (MRU)

This one selection is generally counter intuitive since if you have
a really recent item, you probably want to keep it in the cache.
But for some situations, the most recent can be the items that are
less likely to be reused. You can flip the LRU in an MRU by using
a negative weight.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_recency_weight(-1.0);
    opts.set_eviction_policy(ep);

### Segmented Least Recently Used (SLRU)

This is similar to the MRU, it removes the most recently added items
if they have not been used sufficiently by the time an eviction runs.
Otherwise, the algorithm works like a standard LRU.

    magpie::eviction_policy_slru::pointer_t ep(std::make_shared<magpie::eviction_policy_slru>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_minimum_access_count(5.0);
    opts.set_eviction_policy(ep);

In this example, we assume that any item with an access count of 5 or
less needs to be evicted with a pure FIFO algorithm. The other items
make use of the standard LRU, so equivalent to the following with the
ADD algorithm:

    ep->profile().set_recency_weight(1.0);

**NOTE:** All the FIFO-ed items will appear before the LRU items. So
          if a sufficient number of FIFO items is present, those are
	  evicted and no old item is.

#### Clock

The Clock algorithm is generally used by operating systems managing
memory for processes by setting a bit to 0 or 1 it knows whether to
give the page another chance or not. Say by default you set a page
to 1, the first time the eviction algorithm goes through, the bit
goes to 0 and the page is not evicted. If the page is reused in
the meantime, the bit goes back to 1. If the bit is 0, the eviction
happens immediately. The pages are walked through using the equivalent
of a ring buffer.

In our cache, we do not have such a bit. However, you can simulate
that behavior at the start where an item will not get evicted the
first time it gets hit. This uses the turn counter, the number of
times the eviction algorithm ran against that one item.

    magpie::eviction_policy_slru::pointer_t ep(std::make_shared<magpie::eviction_policy_slru>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_recency_weight(-1.0);
    ep->profile().set_turn_weight(1.0);
    opts.set_eviction_policy(ep);

This code sample sets up your cache as a standard MRU but also
protects brand new items from being evicted too soon.

