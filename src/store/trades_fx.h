/*
 * trades_fx.h - all trades held in memory, prices as fixed-point integers (Leroi)
 *
 * Same idea as trades.h, but price and qty are stored as long long
 * (scaled by PRICE_SCALE and QTY_SCALE from fixed.h) instead of double.
 *
 * One header, three versions. Link exactly ONE of them:
 *   v5_trades_fx_array.c    - fixed-size global array of MAX_TRADES       (V5)
 *   v6_trades_fx_malloc.c   - malloc'd array sized to the file, fgets     (V6)
 *   v7-v8_trades_fx_fread.c - same array, whole file read with one fread  (V7, V8)
 * All hand back the array the same way (Leroi's Stage C interface), so
 * v5-v8_main_stageB_fixed.c is identical for every version; only WHERE the
 * array lives changes.
 */
#ifndef TRADES_FX_H
#define TRADES_FX_H

#include "../engine_fx/fixed.h"
#include "trades.h"            /* for MAX_TRADES */

/* One trade, all whole numbers. Still 3 x 8 bytes + 4 bytes, padded to 32. */
typedef struct {
    long long time;     /* timestamp in microseconds          */
    long long price;    /* price x PRICE_SCALE (cents)        */
    long long qty;      /* qty x QTY_SCALE                    */
    int       is_buy;   /* 1 = buy, 0 = sell                  */
} TradeFx;

/* Read every trade in the file into an array of TradeFx.
 * On success: returns the array and sets *n_out to the number of trades.
 * On error:   returns NULL (nothing is left allocated).
 * Give the array back with free_trades_fx() when finished. */
TradeFx *load_trades_fx(const char *path, long long *n_out);

/* Give back the array from load_trades_fx (does nothing for the global array). */
void free_trades_fx(TradeFx *trades);

#endif /* TRADES_FX_H */
