/*
 * signal_fx.h - crossover BUY / SELL signals using exact whole-number maths
 *
 * The brief defines d = SMA_short - SMA_long, with
 *   BUY  when d(previous) <= 0 and d(now) > 0
 *   SELL when d(previous) >= 0 and d(now) < 0
 *
 * SMA_short = sum_short / S and SMA_long = sum_long / L. Multiplying both
 * sides by S x L (both positive, so the sign doesn't change) gives
 *   d has the same sign as   sum_short x L  -  sum_long x S
 * which is whole-number maths only: no division and no rounding, so when the
 * two averages are equal d is exactly 0.
 */
#ifndef SIGNAL_FX_H
#define SIGNAL_FX_H

#include "../engine/signal.h"  /* for SIG_BUY, SIG_SELL, SIG_NONE */

typedef struct {
    long long prev_d;     /* d on the previous trade (scaled, sign is what matters) */
    int       have_prev;  /* 1 once a previous d exists */
    int       buys;       /* total BUY signals so far   */
    int       sells;      /* total SELL signals so far  */
} CrossoverFx;

void crossover_fx_init(CrossoverFx *x);

/* sum_short / sum_long are the exact window sums, short_win / long_win the
 * window sizes. Returns SIG_BUY, SIG_SELL or SIG_NONE. */
int  crossover_fx_update(CrossoverFx *x, long long sum_short, int short_win,
                         long long sum_long, int long_win);

#endif /* SIGNAL_FX_H */
