
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

|----------------|-----------------|------------------------------------------------|--------------------|
| Strategy Flag  |   Logic Type    |           Who does it protect most?            | Vibe / Personality |
|----------------|-----------------|------------------------------------------------|--------------------|
| ADD            | Balanced OR     | Both, but favors extreme highs.                | Fair & steady      |
| MULTIPLY       | Strict AND      | Items that are active right now.               | Fast & ruthless    |
| MINIMUM        | Bottleneck AND  | Strictly penalizes the weakest metric.         | Ultra-pessimistic  |
| MAXIMUM        | Pure OR         | Equal protection for the fresh and the famous. | Ultra-protective   |
| GEOMETRIC MEAN | Aggressive AND  | Balance which item to use.                     | Weighted ruthless  |
| HARMONIC MEAN  | Fair AND        | Gently penalizes the weakest metric.           | Pessimistic        |
|----------------|-----------------|------------------------------------------------|--------------------|

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

