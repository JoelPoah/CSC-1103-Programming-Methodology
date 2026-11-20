/*
 * signal_fx.c - exact crossover signals (see signal_fx.h)
 */
#include "signal_fx.h"

/* crossover_fx_init
 * What:    no previous d yet, zero counts
 */
void crossover_fx_init(CrossoverFx *x) {
    x->prev_d = 0;
    x->have_prev = 0;
    x->buys = 0;
    x->sells = 0;
}

/* crossover_fx_update
 * What:    work out d for this trade, compare with the previous d
 * Steps:   1) d = sum_short x long_win - sum_long x short_win
 *             (same sign as SMA_short - SMA_long, see signal_fx.h)
 *          2) if there is a previous d:
 *               prev_d <= 0 and d > 0 -> BUY
 *               prev_d >= 0 and d < 0 -> SELL
 *          3) remember d for the next trade
 */
int crossover_fx_update(CrossoverFx *x, long long sum_short, int short_win,
                        long long sum_long, int long_win) {
    long long d = sum_short * long_win - sum_long * short_win;
    int sig = SIG_NONE;

    if (x->have_prev) {
        if (x->prev_d <= 0 && d > 0) {
            sig = SIG_BUY;
            x->buys++;
        } else if (x->prev_d >= 0 && d < 0) {
            sig = SIG_SELL;
            x->sells++;
        }
    }
    x->prev_d = d;
    x->have_prev = 1;
    return sig;
}
