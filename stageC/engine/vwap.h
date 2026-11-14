/*
 * vwap.h - running VWAP (volume-weighted average price)
 *
 * Stage C: copied from stageB/engine/vwap.h (unchanged), the same way Stage B
 * copied Stage A's modules, so Stage C's engine and its assembly live here.
 *
 * VWAP = sum(price * qty) / sum(qty), updated one trade at a time.
 * Only two running sums are kept, so memory use is constant.
 */
#ifndef VWAP_H
#define VWAP_H

typedef struct {
    double sum_pq;   /* running sum of price * qty */
    double sum_q;    /* running sum of qty         */
} Vwap;

void   vwap_init(Vwap *v);
void   vwap_update(Vwap *v, double price, double qty);
double vwap_value(const Vwap *v);

#endif /* VWAP_H */
