/*
 * sma_fx.h - running-sum simple moving average with fixed-point prices
 *
 * Same circular buffer + running sum as sma.h, but the prices and the sum
 * are long long. Adding and subtracting whole numbers is exact, so the
 * running sum never drifts away from the true sum of the window.
 */
#ifndef SMA_FX_H
#define SMA_FX_H

#include "sma.h"               /* for MAX_WIN */

typedef struct {
    long long win[MAX_WIN];  /* circular buffer of the most recent prices */
    int       window;        /* N: how many prices the average covers     */
    int       head;          /* next slot to overwrite                    */
    int       count;         /* prices seen so far, capped at window      */
    long long sum;           /* exact sum of the prices in the buffer     */
} SmaFx;

void      sma_fx_init(SmaFx *s, int window);  /* window must be 1..MAX_WIN */
void      sma_fx_push(SmaFx *s, long long price);
int       sma_fx_ready(const SmaFx *s);       /* 1 once count == window    */
long long sma_fx_sum(const SmaFx *s);         /* exact window sum          */
double    sma_fx_value(const SmaFx *s);       /* average in USDT, printing */

#endif /* SMA_FX_H */
