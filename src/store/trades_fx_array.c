/*
 * trades_fx_array.c - trades in a fixed-size global array (see trades_fx.h)
 */
#include <stdio.h>
#include "trades_fx.h"
#include "../reader/read_trade_fx.h"

/* A global (not a local inside main) because 10 million x 32 bytes =
 * 320 MB, which is far too big for the stack. "static" keeps the name
 * private to this file; the main program reaches it through the pointer
 * that load_trades_fx returns. */
static TradeFx all_trades[MAX_TRADES_FX];

/* load_trades_fx
 * What:    copy every trade from the CSV file into all_trades[]
 * In/Out:  path = the CSV file, *count = number of trades loaded.
 *          Returns the array, or NULL if the file cannot be opened or
 *          has too many trades.
 * Watch:   never write past the end of the array (n must stay < MAX_TRADES_FX)
 */
TradeFx *load_trades_fx(const char *path, long long *count) {
    long long n = 0;
    FILE *trades_file = fopen(path, "r");

    *count = 0;
    if (trades_file == NULL) {
        printf("cannot open %s\n", path);
        return NULL;
    }

    while (n < MAX_TRADES_FX &&
           read_trade_fx(trades_file, &all_trades[n].time, &all_trades[n].price,
                         &all_trades[n].qty, &all_trades[n].is_buy)) {
        n++;
    }

    /* the array is full: is there still a trade left in the file? */
    if (n == MAX_TRADES_FX) {
        TradeFx extra;
        if (read_trade_fx(trades_file, &extra.time, &extra.price, &extra.qty, &extra.is_buy)) {
            printf("too many trades in %s (limit is %d)\n", path, MAX_TRADES_FX);
            fclose(trades_file);
            return NULL;
        }
    }

    fclose(trades_file);
    *count = n;
    return all_trades;
}

/* free_trades_fx
 * What:    nothing to do: a global array lives until the program ends
 */
void free_trades_fx(TradeFx *trades) {
    (void)trades;   /* tells the compiler the parameter is unused on purpose */
}

/* trades_fx_bytes
 * What:    the array is always full size, however small the file is
 */
long long trades_fx_bytes(long long count) {
    (void)count;
    return (long long)sizeof all_trades;
}
