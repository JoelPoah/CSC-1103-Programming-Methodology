/*
 * vwap_fx.h - running VWAP with fixed-point sums
 *
 * Same formula as vwap.h, VWAP = sum(price x qty) / sum(qty), but both sums
 * are whole numbers, so adding millions of trades never builds up rounding
 * error. See fixed.h for why the sums cannot overflow.
 */
#ifndef VWAP_FX_H
#define VWAP_FX_H

typedef struct {
    long long sum_pq;   /* sum of price x qty  (scale PRICE_SCALE x QTY_SCALE) */
    long long sum_q;    /* sum of qty          (scale QTY_SCALE)               */
} VwapFx;

void   vwap_fx_init(VwapFx *v);
void   vwap_fx_update(VwapFx *v, long long price, long long qty);
double vwap_fx_value(const VwapFx *v);   /* in USDT, for printing */

#endif /* VWAP_FX_H */
