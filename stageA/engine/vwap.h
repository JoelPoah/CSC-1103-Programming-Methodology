/*
 * vwap.h - running VWAP (volume-weighted average price)
 *
 * VWAP = sum(price * qty) / sum(qty), updated one trade at a time.
 * Only two running sums are kept, so memory use is constant.
 * The sums live inside vwap.c, so no structs or pointers are needed.
 */
#ifndef VWAP_H
#define VWAP_H

void   vwap_init(void);
void   vwap_update(double price, double qty);
double vwap_value(void);

#endif /* VWAP_H */
