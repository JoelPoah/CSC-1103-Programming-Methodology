/*
 * vwap_fx.h - running VWAP with fixed-point numbers
 *
 * Same job as engine/vwap.h, but price and qty are long long whole numbers
 * (price x PRICE_SCALE, qty x QTY_SCALE, see fixed.h). The two running sums
 * are whole numbers too, so nothing is ever rounded while adding.
 *
 * This header has two versions of the code. The Makefile links exactly one:
 *   engine_fx/vwap_fx.c   - C
 *   engine_asm/vwap_fx.S  - hand-written AArch64 assembly (Raspberry Pi)
 */
#ifndef VWAP_FX_H
#define VWAP_FX_H

void   vwap_fx_init(void);
void   vwap_fx_update(long long price, long long qty);
double vwap_fx_value(void);   /* in USDT, for printing */

#endif /* VWAP_FX_H */
