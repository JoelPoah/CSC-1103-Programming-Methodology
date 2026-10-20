/*
 * signal.c - crossover BUY / SELL signals (see signal.h)
 */
#include "signal.h"

static double prev_short = 0.0;   /* short SMA on the previous trade */
static double prev_long = 0.0;    /* long SMA on the previous trade  */
static int    have_prev = 0;      /* 1 once a previous pair exists   */
static int    buys = 0;           /* total BUY signals so far        */
static int    sells = 0;          /* total SELL signals so far       */

/* crossover_init
 * What:    no previous pair yet, zero counts
 * In/Out:  nothing. Returns nothing.
 */
void crossover_init(void) {
    prev_short = 0.0;
    prev_long = 0.0;
    have_prev = 0;
    buys = 0;
    sells = 0;
}

/* crossover_update
 * What:    decide whether this trade is a crossover, then remember this trade's SMAs
 * In/Out:  sma_short / sma_long = this trade's values.
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE.
 * Steps:   1) if have_prev is 0, store the pair and return SIG_NONE
 *          2) if prev_short <= prev_long and sma_short > sma_long -> BUY, buys++
 *             if prev_short >= prev_long and sma_short < sma_long -> SELL, sells++
 *          3) store this trade's pair as the previous pair, have_prev = 1
 * Watch:   the caller only calls this once both SMAs are ready
 */
int crossover_update(double sma_short, double sma_long) {
    int sig = SIG_NONE;
    if(have_prev == 1){
        if((prev_short <= prev_long) && (sma_short > sma_long)){
            // short crossing above long therefore buy
            sig = SIG_BUY;
            buys++;
        }
        else if ((prev_short >= prev_long) && (sma_short < sma_long)){
            // short crossing below long therefore sell
            sig = SIG_SELL;
            sells++;
        }
    }
    // remember this trade's pair for the next call, whatever happened above
    prev_short = sma_short;
    prev_long = sma_long;
    have_prev = 1;
    return sig;
}

int crossover_buys(void)  { return buys; }
int crossover_sells(void) { return sells; }
