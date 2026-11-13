/*
 * stream_engine.c - Version 0 for the Stage C replay and rival bot (see stream_engine.h)
 */
#include "../../helper_function/trade_io.h"   /* helper library: next_trade() */
#include "fixed.h"                            /* PRICE_SCALE (cents)          */
#include "vwap.h"                             /* Stage C engine modules       */
#include "ohlc.h"                             /* (copied from Stage B)        */
#include "sma.h"
#include "sma_naive.h"                        /* the NAIVE SMA                */
#include "signal.h"
#include "stream_engine.h"

/* The modules' state. "static" means only this file can see these variables. */
static Vwap      vwap;
static OhlcBar   bar;
static OhlcBar   finished;
static Sma       short_sma;
static Sma       long_sma;
static Crossover cross;

/* stream_start
 * What:    open the file and reset VWAP, OHLC, both SMAs and the crossover
 * Returns: 1 if OK, 0 if the file cannot be opened
 */
int stream_start(char path[], int short_win, int long_win) {
    if (!open_trades(path)) {
        return 0;
    }
    vwap_init(&vwap);
    ohlc_init(&bar);
    ohlc_init(&finished);
    sma_init(&short_sma, short_win);
    sma_init(&long_sma, long_win);
    crossover_init(&cross);
    return 1;
}

/* stream_skip
 * What:    read count lines without processing them (for --from-min)
 */
void stream_skip(long long count) {
    long long i;

    for (i = 0; i < count; i++) {
        next_trade();
    }
}

/* stream_step
 * What:    read one line -> VWAP -> OHLC -> naive SMAs -> crossover
 *          (one pass of the naive build of the loop in stageB/stageB_inmem.c)
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE
 */
int stream_step(void) {
    double price;

    if (!next_trade()) {
        return SIG_NONE;                  /* end of the file */
    }
    price = trade_price();
    vwap_update(&vwap, price, trade_qty());
    ohlc_update(&bar, trade_time(), price, &finished);
    sma_naive_push(&short_sma, price);
    sma_naive_push(&long_sma, price);
    if (sma_ready(&short_sma) && sma_ready(&long_sma)) {
        return crossover_update(&cross, sma_naive_value(&short_sma),
                                sma_naive_value(&long_sma));
    }
    return SIG_NONE;
}

/* stream_price_cents
 * What:    the current trade's price in whole cents, e.g. 76589.58 -> 7658958
 * Why +0.5: rounds to the nearest cent, like the fixed-point reader
 */
long long stream_price_cents(void) {
    return (long long)(trade_price() * PRICE_SCALE + 0.5);
}

void stream_stop(void) {
    close_trades();
}

int stream_buys(void) {
    return cross.buys;
}

int stream_sells(void) {
    return cross.sells;
}

double stream_vwap(void) {
    return vwap_value(&vwap);
}
