/*
 * trades_fx.h - all trades held in memory, prices as fixed-point integers
 *
 * Stage C: extended from stageB/engine/trades_fx.h. Same TradeFx struct, but
 * Stage B's fixed-size global array (extern TradeFx trades_fx[MAX_TRADES],
 * 320 MB whatever the file size) is replaced by an array made with malloc
 * to fit the file (brief, Stage C step 1).
 *
 * One header, two loaders. Link exactly ONE of them (see the Makefile):
 *   trades_fx_malloc.c - count the lines, malloc, fill with fgets   (ladder V6)
 *   trades_fx_fread.c  - one fread copies the whole file, then the
 *                        lines are parsed in memory                 (ladder V7)
 * Both parse each line straight to fixed-point with read_trade_fx.c.
 */
#ifndef TRADES_FX_H
#define TRADES_FX_H

#include "fixed.h"

/* One trade, all whole numbers. Still 3 x 8 bytes + 4 bytes, padded to 32. */
typedef struct {
    long long time;     /* timestamp in microseconds          */
    long long price;    /* price x PRICE_SCALE (cents)        */
    long long qty;      /* qty x QTY_SCALE                    */
    int       is_buy;   /* 1 = buy, 0 = sell                  */
} TradeFx;

/* Read every trade in the file into a malloc'd array of TradeFx.
 * On success: returns the array and sets *n_out to the number of trades.
 * On error:   returns NULL (nothing is left allocated).
 * Give the array back with free_trades_fx() when finished. */
TradeFx *load_trades_fx(const char *path, long long *n_out);

/* Give back the array from load_trades_fx. */
void free_trades_fx(TradeFx *trades);

#endif /* TRADES_FX_H */
