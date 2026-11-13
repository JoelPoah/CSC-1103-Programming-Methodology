/*
 * sma_fx.c - running-sum SMA with fixed-point prices (see sma_fx.h)
 *
 * Stage C: copied from stageB/engine/sma_fx.c (unchanged), the same way Stage B
 * copied Stage A's modules, so Stage C's engine and its assembly live here.
 */
#include <stdio.h>
#include "sma_fx.h"
#include "fixed.h"

/* sma_fx_init
 * What:    empty buffer with the given window size
 * Watch:   reject window < 1 or window > MAX_WIN
 */
void sma_fx_init(SmaFx *s, int window) {
    s->head = 0;
    s->count = 0;
    s->sum = 0;
    s->window = window;
    if (window < 1 || window > MAX_WIN) {
        printf("SMA INIT FAILED: window size out of bounds\n");
        s->window = 1;   /* safe value so push never divides by zero */
    }
}

/* sma_fx_push
 * What:    add a price; once the buffer is full, drop the oldest one
 * Steps:   S_new = S_old + newest price - oldest price (one add, one subtract)
 */
void sma_fx_push(SmaFx *s, long long price) {
    if (s->count == s->window) {
        s->sum -= s->win[s->head];      /* oldest price leaves the window */
    }
    s->win[s->head] = price;
    s->sum += price;
    s->head = (s->head + 1) % s->window;
    if (s->count < s->window) {
        s->count++;
    }
}

/* sma_fx_ready
 * What:    has the window filled up yet? Returns 1 if count == window.
 */
int sma_fx_ready(const SmaFx *s) {
    return s->count == s->window;
}

/* sma_fx_sum
 * What:    the exact window sum. The crossover uses this directly so the
 *          main loop never has to divide.
 */
long long sma_fx_sum(const SmaFx *s) {
    return s->sum;
}

/* sma_fx_value
 * What:    the average in USDT = sum / N / PRICE_SCALE, only for printing
 * Watch:   returns 0.0 until the window is full
 */
double sma_fx_value(const SmaFx *s) {
    if (!sma_fx_ready(s)) {
        return 0.0;
    }
    return (double)s->sum / s->window / PRICE_SCALE;
}
