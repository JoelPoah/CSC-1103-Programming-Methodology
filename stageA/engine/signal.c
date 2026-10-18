/*
 * signal.c - crossover BUY / SELL signals (see signal.h)
 */
#include "signal.h"

/* crossover_init
 * What:    no previous pair yet, zero counts
 * In/Out:  x = the Crossover to reset. Returns nothing.
 */
void crossover_init(Crossover *x) {
    /* TODO */
    x->prev_short = 0.0;
    x->prev_long = 0.0;
    x->have_prev = 0;
    x->buys = 0;
    x->sells = 0;
    
}

/* crossover_update
 * What:    decide whether this trade is a crossover, then remember this trade's SMAs
 * In/Out:  x = the Crossover state, sma_short / sma_long = this trade's values.
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE.
 * Steps:   1) if have_prev is 0, store the pair and return SIG_NONE
 *          2) if prev_short <= prev_long and sma_short > sma_long -> BUY, buys++
 *             if prev_short >= prev_long and sma_short < sma_long -> SELL, sells++
 *          3) store this trade's pair as the previous pair, have_prev = 1
 * Watch:   the caller only calls this once both SMAs are ready
 */
int crossover_update(Crossover *x, double sma_short, double sma_long) {
    /* TODO */
    if(x->have_prev == 0){
        x->prev_short = sma_short;
        x->prev_long = sma_long;
        x->have_prev = 1;
        return SIG_NONE; //0
    }
    else if((x->prev_short <= x->prev_long) && (sma_short > sma_long)){
        // short crossing above long therefore buy
        x->prev_short = sma_short;
        x->prev_long = sma_long;
        x->have_prev = 1;
       return SIG_BUY;
    }
    else if ((x->prev_short >= x->prev_long) && (sma_short < sma_long)){
        // short crossing below long therefore sell
        x->prev_short = sma_short;
        x->prev_long = sma_long;
        x->have_prev = 1;
        return SIG_SELL;
    }
}

/* signal_name
 * What:    text for a signal value, used in the trade table
 * In/Out:  sig = SIG_BUY, SIG_SELL or SIG_NONE. Returns a string literal.
 */
const char *signal_name(int sig) {
    /* TODO */
    char *string = ""; 
    switch (sig)
    {
    case SIG_SELL:
        string = "SELL";
        break;
    case SIG_BUY:
        string = "BUY";
        break;
    default:
        string = "";
    }
    return string;
}
