/*
 * sma_naive.c - slow simple moving average (see sma_naive.h)
 *
 * Stage C: copied from stageB/engine/sma_naive.c (unchanged), the same way Stage B
 * copied Stage A's modules, so Stage C's engine and its assembly live here.
 */
#include "sma_naive.h"

/* sma_naive_push
 * What:    put the new price in the circular buffer (overwriting the oldest)
 * In/Out:  s = the Sma to update, price = new price. Returns nothing.
 * Note:    unlike sma_push, this does NOT keep s->sum up to date;
 *          the sum is worked out from scratch in sma_naive_value instead.
 */
void sma_naive_push(Sma *s, double price) {
    s->win[s->head] = price;
    s->head = (s->head + 1) % s->window;   /* wrap back to slot 0 at the end */
    if (s->count < s->window) {
        s->count++;
    }
}

/* sma_naive_value
 * What:    add up every price in the window, then divide by N
 * In/Out:  s = the Sma (read only). Returns the average, or 0.0 if the
 *          window is not full yet.
 * Cost:    N additions on every call -> this is the slow part we measure.
 * Note:    the order of the prices in the buffer doesn't matter for a sum,
 *          so we can simply add win[0] .. win[N-1].
 */
double sma_naive_value(const Sma *s) {
    double total = 0.0;
    int i;

    if (!sma_ready(s)) {
        return 0.0;
    }
    for (i = 0; i < s->window; i++) {
        total += s->win[i];
    }
    return total / s->window;
}
