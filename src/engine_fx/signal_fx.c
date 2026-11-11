/*
 * signal_fx.c - crossover BUY / SELL signals from window sums (see signal_fx.h)
 */
#include "signal_fx.h"

static long long prev_diff = 0;   /* diff on the previous trade    */
static int       have_prev = 0;   /* 1 once a previous diff exists */
static int       buys = 0;        /* total BUY signals so far      */
static int       sells = 0;       /* total SELL signals so far     */

/* crossover_fx_init
 * What:    no previous diff yet, zero counts
 */
void crossover_fx_init(void) {
    prev_diff = 0;
    have_prev = 0;
    buys = 0;
    sells = 0;
}

/* crossover_fx_update
 * What:    decide whether this trade is a crossover, then remember its diff
 * In/Out:  the two exact window sums and their window sizes.
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE.
 * Steps:   1) diff = sum_short * long_win - sum_long * short_win
 *          2) if there is a previous diff:
 *               prev_diff <= 0 and diff > 0 -> BUY,  buys++
 *               prev_diff >= 0 and diff < 0 -> SELL, sells++
 *          3) store diff as the previous diff, have_prev = 1
 * Watch:   the caller only calls this once both SMAs are ready
 */
int crossover_fx_update(long long sum_short, int short_win,
                        long long sum_long, int long_win) {
    long long diff = sum_short * long_win - sum_long * short_win;
    int sig = SIG_NONE;
    if(have_prev == 1){
        if((prev_diff <= 0) && (diff > 0)){
            // short crossing above long therefore buy
            sig = SIG_BUY;
            buys++;
        }
        else if ((prev_diff >= 0) && (diff < 0)){
            // short crossing below long therefore sell
            sig = SIG_SELL;
            sells++;
        }
    }
    // remember this trade's diff for the next call, whatever happened above
    prev_diff = diff;
    have_prev = 1;
    return sig;
}

int crossover_fx_buys(void)  { return buys; }
int crossover_fx_sells(void) { return sells; }
