/*
 * vwap_fx.c - running VWAP with fixed-point numbers (see vwap_fx.h)
 */
#include "vwap_fx.h"
#include "fixed.h"

/* the two running sums, private to this file */
static long long sum_pq = 0;   /* running sum of price * qty (scale PRICE_SCALE x QTY_SCALE) */
static long long sum_q  = 0;   /* running sum of qty         (scale QTY_SCALE)               */

/* vwap_fx_init
 * What:    set both sums to zero before the first trade
 */
void vwap_fx_init(void) {
    sum_pq = 0;
    sum_q = 0;
}

/* vwap_fx_update
 * What:    add one trade to the running sums
 * In/Out:  price and qty = the trade, as scaled whole numbers.
 * Watch:   fixed.h shows that price * qty and the whole day's sum both
 *          fit in a long long with room to spare
 */
void vwap_fx_update(long long price, long long qty) {
    sum_pq += price * qty;
    sum_q += qty;
}

/* vwap_fx_value
 * What:    the current VWAP in USDT = sum_pq / sum_q / PRICE_SCALE
 *          (the QTY_SCALE on top and bottom cancels out)
 * Watch:   the only place a double appears, and only for printing.
 *          sum_q can be 0 if no quantity has been added yet
 */
double vwap_fx_value(void) {
    if (sum_q > 0){
        double VWAP = (double)sum_pq / (double)sum_q / PRICE_SCALE;
        return VWAP;
    }
    return 0.0;
}
