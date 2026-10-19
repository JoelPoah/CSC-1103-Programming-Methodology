/*
 * vwap.c - running VWAP (see vwap.h)
 */
#include "vwap.h"

/* the two running sums, private to this file */
static double sum_pq = 0.0;   /* running sum of price * qty */
static double sum_q  = 0.0;   /* running sum of qty         */

/* vwap_init
 * What:    set both sums to zero before the first trade
 * In/Out:  nothing. Returns nothing.
 * Watch:   vwap_value must not divide by zero before any trade is added
 */
void vwap_init(void) {
    sum_pq = 0.0;
    sum_q = 0.0;
}

/* vwap_update
 * What:    add one trade to the running sums
 * In/Out:  price and qty = the trade. Returns nothing.
 * Steps:   1) sum_pq += price * qty
 *          2) sum_q  += qty
 */
void vwap_update(double price, double qty) {
    sum_pq += price * qty;
    sum_q += qty;
}

/* vwap_value
 * What:    the current VWAP = sum_pq / sum_q
 * In/Out:  nothing. Returns the VWAP as a double.
 * Watch:   sum_q can be 0 if no quantity has been added yet
 */
double vwap_value(void) {
    if (sum_q > 0){
        double VWAP = sum_pq / sum_q;
        return VWAP;
    }
    return 0.0;
}
