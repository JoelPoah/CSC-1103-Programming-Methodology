/*
 * mem_engine.c - the fastest version for the Stage C replay (see mem_engine.h)
 */
#include "../../stageB/engine/fixed.h"        /* PRICE_SCALE, QTY_SCALE */
#include "../../stageB/engine/vwap_fx.h"      /* Leroi's Stage B modules */
#include "../../stageB/engine/ohlc_fx.h"
#include "../../stageB/engine/sma_fx.h"
#include "../../stageB/engine/signal_fx.h"
#include "mem_engine.h"

/* The modules' state. "static" means only this file can see these variables. */
static VwapFx      vwap;
static OhlcBarFx   bar;
static OhlcBarFx   finished;
static SmaFx       short_sma;
static SmaFx       long_sma;
static CrossoverFx cross;
static int         short_window;
static int         long_window;

/* to_cents / to_qty_units
 * What:    turn a decimal into a scaled whole number, exactly like Leroi's
 *          to_fixed() in stageB/engine/trades_fx.c (+0.5 rounds to the
 *          nearest whole number instead of chopping off the fraction)
 */
long long to_cents(double price) {
    return (long long)(price * PRICE_SCALE + 0.5);
}

long long to_qty_units(double qty) {
    return (long long)(qty * QTY_SCALE + 0.5);
}

/* mem_start
 * What:    reset VWAP, OHLC, both SMAs and the crossover
 */
void mem_start(int short_win, int long_win) {
    short_window = short_win;
    long_window = long_win;
    vwap_fx_init(&vwap);
    ohlc_fx_init(&bar);
    ohlc_fx_init(&finished);
    sma_fx_init(&short_sma, short_win);
    sma_fx_init(&long_sma, long_win);
    crossover_fx_init(&cross);
}

/* mem_step
 * What:    one trade: convert to whole numbers -> VWAP -> OHLC ->
 *          running-sum SMAs -> exact crossover (same steps as Leroi's
 *          stageB/stageB_fixed.c)
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE
 */
int mem_step(Trade *t) {
    long long price = to_cents(t->price);
    long long qty = to_qty_units(t->qty);

    vwap_fx_update(&vwap, price, qty);
    ohlc_fx_update(&bar, t->time, price, &finished);
    sma_fx_push(&short_sma, price);
    sma_fx_push(&long_sma, price);
    if (sma_fx_ready(&short_sma) && sma_fx_ready(&long_sma)) {
        return crossover_fx_update(&cross, sma_fx_sum(&short_sma), short_window,
                                   sma_fx_sum(&long_sma), long_window);
    }
    return SIG_NONE;
}

long long mem_price_cents(Trade *t) {
    return to_cents(t->price);
}

int mem_buys(void) {
    return cross.buys;
}

int mem_sells(void) {
    return cross.sells;
}

double mem_vwap(void) {
    return vwap_fx_value(&vwap);
}
