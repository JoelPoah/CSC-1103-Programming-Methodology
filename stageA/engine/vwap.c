/*
 * vwap.c - running VWAP (see vwap.h)
 */
#include "vwap.h"

/* vwap_init
 * What:    set both sums to zero before the first trade
 * In/Out:  v = the Vwap to reset. Returns nothing.
 * Watch:   vwap_value must not divide by zero before any trade is added
 */
void vwap_init(Vwap *v) {
    /* TODO */
    v->sum_pq = 0.0;
    v->sum_q = 0.0;
}

/* vwap_update
 * What:    add one trade to the running sums
 * In/Out:  v = the Vwap, price and qty = the trade. Returns nothing.
 * Steps:   1) sum_pq += price * qty
 *          2) sum_q  += qty
 */
void vwap_update(Vwap *v, double price, double qty) {
    /* TODO */
    v->sum_pq += price * qty;
    v->sum_q += qty;
}

/* vwap_value
 * What:    the current VWAP = sum_pq / sum_q
 * In/Out:  v = the Vwap (read only). Returns the VWAP as a double.
 * Watch:   sum_q can be 0 if no quantity has been added yet
 */
double vwap_value(const Vwap *v) {
    /* TODO */
    if (v->sum_pq > 0.0 && v->sum_q >0){
    double VWAP = v->sum_pq / v->sum_q;
    return VWAP;
    }
    return 0.0;
}
