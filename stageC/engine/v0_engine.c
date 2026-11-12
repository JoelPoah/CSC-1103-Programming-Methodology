/*
 * v0_engine.c - Version 0 for the Stage C replay and rival bot (see v0_engine.h)
 */
#include "../../helper_function/trade_io.h"   /* Stage A helper library */
#include "../../stageA/engine/vwap.h"         /* Stage A modules        */
#include "../../stageA/engine/ohlc.h"
#include "../../stageA/engine/sma.h"          /* the NAIVE SMA          */
#include "../../stageA/engine/signal.h"
#include "../../stageB/engine/fixed.h"        /* PRICE_SCALE (cents)    */
#include "v0_engine.h"

/* v0_start
 * What:    open the file and reset VWAP, OHLC, both SMAs and the crossover
 * Returns: 1 if OK, 0 if the file cannot be opened
 */
int v0_start(char path[], int short_win, int long_win) {
    if (!open_trades(path)) {
        return 0;
    }
    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();
    return 1;
}

/* v0_skip
 * What:    read count lines without processing them (for --from-min)
 */
void v0_skip(long long count) {
    long long i;

    for (i = 0; i < count; i++) {
        next_trade();
    }
}

/* v0_step
 * What:    the same steps as one pass of the loop in stageA_manual.c:
 *          read one line -> VWAP -> OHLC -> naive SMAs -> crossover
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE
 */
int v0_step(void) {
    int signal = SIG_NONE;

    if (!next_trade()) {
        return SIG_NONE;                  /* end of the file */
    }
    vwap_update(trade_price(), trade_qty());
    ohlc_update(trade_time(), trade_price());
    sma_push(SHORT_SMA, trade_price());
    sma_push(LONG_SMA, trade_price());
    if (sma_ready(SHORT_SMA) && sma_ready(LONG_SMA)) {
        signal = crossover_update(sma_value(SHORT_SMA), sma_value(LONG_SMA));
    }
    return signal;
}

/* v0_price_cents
 * What:    the current trade's price in whole cents, e.g. 76589.58 -> 7658958
 * Why +0.5: same rounding as Leroi's to_fixed() in stageB/engine/trades_fx.c
 */
long long v0_price_cents(void) {
    return (long long)(trade_price() * PRICE_SCALE + 0.5);
}

void v0_stop(void) {
    close_trades();
}

int v0_buys(void) {
    return crossover_buys();
}

int v0_sells(void) {
    return crossover_sells();
}

double v0_vwap(void) {
    return vwap_value();
}
