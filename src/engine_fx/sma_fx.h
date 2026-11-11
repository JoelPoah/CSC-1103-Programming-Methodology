/*
 * sma_fx.h - running-sum SMA with fixed-point prices
 *
 * Same job as engine/sma.h (running-sum version), but the prices and the
 * window sum are long long whole numbers, so the sum is always EXACT:
 * adding and subtracting whole numbers never rounds, so it cannot drift.
 * There are two windows, picked by an id: SHORT_SMA_FX or LONG_SMA_FX.
 *
 * This header has two versions of the code. The Makefile links exactly one:
 *   engine_fx/sma_fx.c   - C
 *   engine_asm/sma_fx.S  - hand-written AArch64 assembly (Raspberry Pi)
 */
#ifndef SMA_FX_H
#define SMA_FX_H

#define MAX_WIN_FX 256            /* largest window the array can hold */

#define SHORT_SMA_FX 0            /* id of the short window */
#define LONG_SMA_FX  1            /* id of the long window  */
#define SMA_FX_COUNT 2            /* how many windows exist */

void      sma_fx_init(int id, int window);   /* window must be 1..MAX_WIN_FX */
void      sma_fx_push(int id, long long price);
int       sma_fx_ready(int id);              /* 1 once count == window       */
long long sma_fx_sum(int id);                /* exact window sum (scaled)    */
double    sma_fx_value(int id);              /* average in USDT, for printing */

#endif /* SMA_FX_H */
