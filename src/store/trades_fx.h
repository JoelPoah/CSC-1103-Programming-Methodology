/*
 * trades_fx.h - all trades held in memory as fixed-point numbers (Stage B / C)
 *
 * This header has two versions of the code. The Makefile links exactly one:
 *   trades_fx_array.c  - Stage B: a fixed-size global array (MAX_TRADES_FX slots)
 *   trades_fx_malloc.c - Stage C: an array made with malloc to fit the file exactly
 * The main program is the same for both: it gets a pointer to the first
 * trade and the number of trades, and gives the array back at the end.
 */
#ifndef TRADES_FX_H
#define TRADES_FX_H

/* stress_full.csv has 8,928,944 trades, so 10 million leaves some room */
#define MAX_TRADES_FX 10000000

/* One trade, every number a whole number. sizeof(TradeFx) = 32 bytes:
 * three 8-byte fields, a 4-byte int and 4 bytes of padding. */
typedef struct {
    long long time;     /* timestamp in microseconds          */
    long long price;    /* price x PRICE_SCALE (see fixed.h)  */
    long long qty;      /* qty   x QTY_SCALE                  */
    int       is_buy;   /* 1 = buy, 0 = sell                  */
} TradeFx;

/* Read every trade in the file into memory.
 *   *count = how many trades were loaded
 * Returns a pointer to the first trade, or NULL on error. */
TradeFx *load_trades_fx(const char *path, long long *count);

/* Give the array back when finished (does nothing for the global array). */
void free_trades_fx(TradeFx *trades);

/* How many bytes the array takes up, for the memory comparison. */
long long trades_fx_bytes(long long count);

#endif /* TRADES_FX_H */
