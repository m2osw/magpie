
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

## Variables

### Timestamps

The algorithms have access to many Unix timestamps (`time_t`).

    ---+----+----+----+----+----+----+----+----+----+----+--... future ...--+----+--...
       |    |    |    |    |    |    |    |    |    |    |                  |    |
       |    |    |    |    |    |    |    |    |    |    |                  |    +---> [M] furthest item timeout (cycle: f_item_furthest_timeout)
       |    |    |    |    |    |    |    |    |    |    |                  |
       |    |    |    |    |    |    |    |    |    |    +---> now          +---> [P] item purge time (item: f_timeout)
       |    |    |    |    |    |    |    |    |    |
       |    |    |    |    |    |    |    |    |    +---> [E] eviction start time (cycle: f_started_on)
       |    |    |    |    |    |    |    |    |
       |    |    |    |    |    |    |    |    +---> [R] last item touch (cycle: f_item_most_recent_touch)
       |    |    |    |    |    |    |    |
       |    |    |    |    |    |    |    +---> [U] last item update (cycle: f_item_most_recent_update)
       |    |    |    |    |    |    |
       |    |    |    |    |    |    +---> [T] item last touched (item: f_last_touched_on)
       |    |    |    |    |    |
       |    |    |    |    |    +---> [L] item last updated (item: f_last_updated_on)
       |    |    |    |    |
       |    |    |    |    +---> [Q] oldest touch (cycle: f_item_oldest_touch)
       |    |    |    |
       |    |    |    +---> [O] oldest update (cycle: f_item_oldest_update)
       |    |    |
       |    |    +---> [C] item created on (item: f_created_on)
       |    |
       |    +---> [F] first created item (item: f_item_oldest_created_on)
       |
       +---> [S] application start time (magpie_start_time() -- application wide)

Some variables can be expressed as follow:

$$\begin{aligned}
M = \max(P_1, P_2, \dots, P_n)\\
R = \max(T_1, T_2, \dots, T_n)\\
U = \max(L_1, L_2, \dots, L_n)\\
Q = \min(T_1, T_2, \dots, T_n)\\
O = \min(L_1, L_2, \dots, L_n)
\end{aligned}$$


The order is what is most likely. In this drawing, the interval between
each item is kept equal, but it is not the case in the real world. Not
only that, since item specific timestamps can be updated **after** a
new eviction starts, it can actually appear after that time
(i.e. `f_started_on < f_item_most_recent_update` can be true).

### Other Variables (Counters, byte sizes, etc.)

The eviction system also uses several counters and some other variables
like so:

    [A] item access count (item: f_access_count)
    [B] item with the largets allocated memory (cycle: f_item_maximum_allocated)
    [G] item cost (item: f_cost)
    [H] item with the highest turn (cycle: f_item_highest_turn)
    [K] maximum access count (cycle: f_item_maximum_access_count)
    [Y] item allocated memory (item: f_allocated)
    [N] item eviction turn (item: f_turn)
    [Z] number of cycles (cycle: f_turn)

Some variables can be expressed as follow:

$$\begin{aligned}
B = \max(Y_{1}, Y_{2}, \dots, Y_{n})\\
H = \max(N_{1}, N_{2}, \dots, N_{n})\\
K = \max(A_{1}, A_{2}, \dots, A_{n})
\end{aligned}$$

### Setup for Survival mode (TTL extension)

The TTL mode is most often in addition to the LRU or LFU setup. This
mode computes a timeout for each time. Once `now >= timeout` is
detected, that object gets evicted.

A get() against an object which timeout is in the past also gets
deleted by default. You can tweak the preferences to instead get
the stale or just not return but also not delete the value.

At times, though, the TTL will not kick in and yet the cache is full.
In that case, if you are not use the LRU or LFU, you can still have
the oldest item removed from the cache early (before their timeout
was reached). To do so, use the survival parameter:

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_survival_weight(1.0);
    opts.set_eviction_policy(ep);

That way, if the purge of old item doesn't do anything (or not enough)
the eviction policy picks up where it left off and still deletes up to
10% of the entries.

Contrary to the FIFO mode, this setup takes the timeout in account so if
some items have longer or shorter TTLs, that's properly taken in account
and those items with a longer TTL are more likely to be kept longer than
those with a shorter TTL.

Equation:

$$W_{eviction} = W_{\text{survival}}\frac{P - E}{M - E}$$

### Setup for LRU mode

The ADD algorithm can be used to setup the eviction mode to LRU.

We support the standard LRU which uses the date of the last access to
an item in the cache, whether that object was added or updated (set),
or whether it was retrieved (get).

Since we keep the dates separate, you can also use the recency write
(set) or the recency read (get) on their own or in an uneven mix.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    if(standard_lru) // max(get, set)
    {
        ep->profile().set_recency_weight(1.0);
    }
    else if(write_lru) // sets only
    {
        ep->profile().set_recency_write_weight(1.0);
    }
    else if(read_lru) // gets only
    {
        ep->profile().set_recency_read_weight(1.0);
    }
    else // privilege reads (get)
    {
        ep->profile().set_recency_write_weight(0.25);
        ep->profile().set_recency_read_weight(0.75);
    }
    opts.set_eviction_policy(ep);

Equations:

$$\begin{aligned}
W_{eviction} = W_{recency}\frac{\max(L, T) - \min(O, Q)}{\max(U, R) - \min(O, Q)}\\
W_{eviction} = W_{\text{recency write}}\frac{L - O}{U - O}\\
W_{eviction} = W_{\text{recency read}}\frac{T - Q}{R - Q}
\end{aligned}$$

The default is to use the most recent of either "updated" or "touched".
This is the standard LRU which does not make a distinction between
a set and a get (i.e. last access on a Unix file).

The "write" recency takes the "last updated" and "oldest update" values
only. It completely ignores the touch timestamps.

Similarly, the "read" recency takes the "last touched" and "oldest touch"
values only. It completely ignores the update timestamps.

As shown in the example above, we can mix the write and the read by
changing the weight of each one. That is similar (but not 100% equivalent)
to using the default LRU (with the `set_recency_weight()` function) but
with the ability to give the write or the read a higher priority.

**Note:** Our LRU runs until a certain amount of items or memory were
          released to satisfy the cache size limits. This is at times
          called a Weithed LRU (W-LRU),

### Setup for LFU mode

The ADD algorithm can be used to setup the eviction mode to LRU.

    magpie::eviction_policy_add::pointer_t ep(std::make_shared<magpie::eviction_policy_add>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_access_count_weight(1.0);
    opts.set_eviction_policy(ep);

Equation:

$$W_{eviction} = W_{\text{access count}}\frac{A}{K}$$

**Note:** Our LFU runs until a certain amount of items or memory were
          released to satisfy the cache size limits. This is at times
          called a Weithed LFU (W-LFU),

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

Equation:

$$W_{eviction} = W_{recency}\frac{\max(L, T) - \min(O, Q)}{\max(U, R) - \min(O, Q)} + W_{\text{access count}}\frac{A}{K}$$

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

**Note:** The FIFO mode makes use of the date when the item was created.
          The precision is one second. If your cache is really fast
          creating many items within one second, then this algorithm
          may not be well adapted for your needs.

Equation:

$$W_{eviction} = W_{\text{age weight}}\frac{E-C}{E-F}$$

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

Equation:

$$W_{eviction} = W_{random} \text{random}(0, 1)$$

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

Equation:

As expected, we reuse the exact same equation as the LRU:

$$W_{eviction} = W_{recency}\frac{\max(L, T) - \min(O, Q)}{\max(U, R) - \min(O, Q)}$$

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

Equation:

$$W_{eviction} = \begin{cases}
\frac{E-C}{E-F} & \text{access count} <= 5\\
\frac{T - O}{R - O} + 2 & \text{otherwise}
\end{cases}$$

**Note:** The `... + 2` in the second equation is to always beat the first
          equation. That way, as long as you have fresh items, they are
          going to be evicted first no matter what. Also, the age and
          recency weights do not get used.

#### Greedy Dual-Size Frequency (GDSF)

The Greed Dual-Size Frequency algorithm implements a duality between the
size and the frequency of the items in the cache.

$$H_{i} = L + \frac{Frequency_{i} \times Costs}{Size_{i}}$$

Where $L = 1.0$ at first and then it gets set to $H_{i}$. Since we do
not support dynamic loops like this, we do not support this exact
algorithm.

However, we can use a slightly different equation allowing for
a decay over time and still mixes the frequency and size like so:

$$\Large{W_{eviction} = \underbrace{\frac{\frac{A}{K} \times G}{\ln(Y)}}_{\text{asset value}} - \underbrace{W_{decay} \frac{E - \max(L, T)}{E - \min(U, R)}}_{\text{idle penalty}}}$$

Since the $W_{decay}$ weight represents an age, this algorithm uses
the age weight. Here is the setup you can use:

    magpie::eviction_policy_slru::pointer_t ep(std::make_shared<magpie::eviction_policy_gdsf>());
    ep->profile().set_size_eviction_trigger_percent(0.95);
    ep->profile().set_size_eviction_reduction_percent(0.9);
    ep->profile().set_age_weight(1.0);
    opts.set_eviction_policy(ep);

If you do not want the asset penalty, set the age weight to 0.0. It then
only takes the asset value in consideration. The problem with this setup
is that once an item grows strong, even if not used anymore for a very
long period of time, it stays in your cache.

The cost is defined by you when you set() the item in the cache.
It should be a number from 0.0 to 1.0. This can be a constant representing
the cost of the source (i.e. 0.01 for SSD, 0.05 for HDD, 0.5 for local
network, and 1.0 for remote network). If the time to fetch the item can
vary greatly, it is suggested to use the Exponentially Weighted Moving
Average (EWMA) equation instead of using the last time on its own:

$$cost_{new} = weight \times cost_{new} + (1 - weight) \times cost_{old}$$

So if fetching data may take from 1ms to 10s, you should clamp the value
and then divide by 10 to normalize it to 0 to 1.0. Once you've done that,
apply the EWMA equation and use the result in the cache::set() call.

Something like this:

    snapdev::timespec_ex const start(snapdev::now());
    ...retrieve new data...
    snapdev::timespec_ex const duration(snapdev::now() - start);
    double const cost_new(std::min(duration.to_sec() / 10.0, 1.0));
    double const cost_old(...retrieve this value from somewhere...);
    double const ewma(weight * cost_new + (1.0 - weight) * cost_old);

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

Equation:

$$W_{eviction} = W_{recency}\frac{\max(L, T) - \min(O, Q)}{\max(U, R) - \min(O, Q)} + W_{\text{turn weight}}\frac{N}{H}$$

<!--
vim: ts=4 sw=4 et
-->
