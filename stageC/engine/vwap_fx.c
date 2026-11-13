/*
 * vwap_fx.c - running VWAP with fixed-point sums (see vwap_fx.h)
 *
 * Stage C: copied from stageB/engine/vwap_fx.c (unchanged), the same way Stage B
 * copied Stage A's modules, so Stage C's engine and its assembly live here.
 */
#include "vwap_fx.h"
#include "fixed.h"

/* vwap_fx_init
 * What:    set both sums to zero before the first trade
 */
void vwap_fx_init(VwapFx *v) {
    v->sum_pq = 0;
    v->sum_q = 0;
}

/* vwap_fx_update
 * What:    add one trade to the running sums (whole-number maths only)
 * In/Out:  price = price x PRICE_SCALE, qty = qty x QTY_SCALE
 */
void vwap_fx_update(VwapFx *v, long long price, long long qty) {
    v->sum_pq += price * qty;
    v->sum_q += qty;
}

/* vwap_fx_value
 * What:    the VWAP in USDT, only used for printing
 * Units:   sum_pq / sum_q gives cents (the QTY_SCALE cancels out),
 *          then divide by PRICE_SCALE to get USDT.
 * Watch:   returns 0.0 before any trade, instead of dividing by zero
 */
double vwap_fx_value(const VwapFx *v) {
    if (v->sum_q == 0) {
        return 0.0;
    }
    return (double)v->sum_pq / (double)v->sum_q / PRICE_SCALE;
}
