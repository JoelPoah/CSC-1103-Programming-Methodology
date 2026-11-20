/*
 * ohlc_fx.h - one-second OHLC bars with fixed-point prices
 *
 * Same as ohlc.h, but open/high/low/close are long long (cents).
 */
#ifndef OHLC_FX_H
#define OHLC_FX_H

#include "ohlc.h"              /* for MICROS_PER_SEC */

typedef struct {
    long long sec;      /* which second this bar covers    */
    long long open;     /* first price in the second       */
    long long high;     /* highest price in the second     */
    long long low;      /* lowest price in the second      */
    long long close;    /* last price in the second        */
    int       n;        /* number of trades in the second  */
    int       active;   /* 1 once a bar has been started   */
} OhlcBarFx;

void ohlc_fx_init(OhlcBarFx *bar);

/* Returns 1 if this trade closed the previous bar (copied to *finished). */
int  ohlc_fx_update(OhlcBarFx *bar, long long time_us, long long price,
                    OhlcBarFx *finished);

#endif /* OHLC_FX_H */
