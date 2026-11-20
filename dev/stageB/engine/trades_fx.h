/*
 * trades_fx.h - all trades held in memory, prices as fixed-point integers
 *
 * Same idea as trades.h, but price and qty are stored as long long
 * (scaled by PRICE_SCALE and QTY_SCALE from fixed.h) instead of double.
 */
#ifndef TRADES_FX_H
#define TRADES_FX_H

#include "fixed.h"
#include "trades.h"            /* for MAX_TRADES */

/* One trade, all whole numbers. Still 3 x 8 bytes + 4 bytes, padded to 32. */
typedef struct {
    long long time;     /* timestamp in microseconds          */
    long long price;    /* price x PRICE_SCALE (cents)        */
    long long qty;      /* qty x QTY_SCALE                    */
    int       is_buy;   /* 1 = buy, 0 = sell                  */
} TradeFx;

extern TradeFx trades_fx[MAX_TRADES];

/* Read every trade in the file into trades_fx[], converting price and qty
 * to fixed-point. Returns how many trades were loaded, or -1 on error. */
long long load_trades_fx(const char *path);

#endif /* TRADES_FX_H */
