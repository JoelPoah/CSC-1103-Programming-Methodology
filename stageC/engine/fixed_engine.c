/*
 * fixed_engine.c - the fastest version for the Stage C replay (see fixed_engine.h)
 */
#include "vwap_fx.h"            /* Stage C engine modules (from Stage B) */
#include "ohlc_fx.h"
#include "sma_fx.h"
#include "signal_fx.h"
#include "fixed_engine.h"

/* The modules' state. "static" means only this file can see these variables. */
static VwapFx      vwap;
static OhlcBarFx   bar;
static OhlcBarFx   finished;
static SmaFx       short_sma;
static SmaFx       long_sma;
static CrossoverFx cross;
static int         short_window;
static int         long_window;

/* fixed_start
 * What:    reset VWAP, OHLC, both SMAs and the crossover
 */
void fixed_start(int short_win, int long_win) {
    short_window = short_win;
    long_window = long_win;
    vwap_fx_init(&vwap);
    ohlc_fx_init(&bar);
    ohlc_fx_init(&finished);
    sma_fx_init(&short_sma, short_win);
    sma_fx_init(&long_sma, long_win);
    crossover_fx_init(&cross);
}

/* fixed_step
 * What:    one trade: VWAP -> OHLC -> running-sum SMAs -> exact crossover
 *          (one pass of the loop in stageB/stageB_fixed.c).
 *          The price is already in cents, so there is no conversion here.
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE
 */
int fixed_step(const TradeFx *t) {
    vwap_fx_update(&vwap, t->price, t->qty);
    ohlc_fx_update(&bar, t->time, t->price, &finished);
    sma_fx_push(&short_sma, t->price);
    sma_fx_push(&long_sma, t->price);
    if (sma_fx_ready(&short_sma) && sma_fx_ready(&long_sma)) {
        return crossover_fx_update(&cross, sma_fx_sum(&short_sma), short_window,
                                   sma_fx_sum(&long_sma), long_window);
    }
    return SIG_NONE;
}

long long fixed_price_cents(const TradeFx *t) {
    return t->price;
}

int fixed_buys(void) {
    return cross.buys;
}

int fixed_sells(void) {
    return cross.sells;
}

double fixed_vwap(void) {
    return vwap_fx_value(&vwap);
}
