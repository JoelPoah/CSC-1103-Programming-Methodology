/*
 * v5_trades_fx_array.c - fixed-point trades in a fixed-size global array (Leroi, V5)
 *
 * Same as stageB/engine/trades_fx.c, except the line is parsed straight to
 * fixed-point by read_trade_fx() (Lucas), so no double is ever created.
 */
#include <stdio.h>
#include "trades_fx.h"
#include "../reader/read_trade_fx.h"

/* Global for the same reason as trades[]: 320 MB is too big for the stack. */
static TradeFx trades_fx[MAX_TRADES];

/* load_trades_fx
 * What:    copy every trade from the CSV into trades_fx[] as fixed-point
 * In/Out:  path = the CSV file, *n_out = number of trades loaded.
 *          Returns trades_fx, or NULL if the file cannot be opened, has a
 *          bad row, or has too many trades.
 * Watch:   never write past the end of the array (n must stay < MAX_TRADES)
 */
TradeFx *load_trades_fx(const char *path, long long *n_out) {
    long long n = 0;
    int status;
    TradeFx t;
    FILE *fp = fopen(path, "r");

    *n_out = 0;
    if (fp == NULL) {
        printf("cannot open %s\n", path);
        return NULL;
    }

    while ((status = read_trade_fx(fp, &t)) == 1) {
        if (n >= MAX_TRADES) {
            /* stop instead of writing outside the array */
            printf("too many trades in %s (limit is %d)\n", path, MAX_TRADES);
            fclose(fp);
            return NULL;
        }
        trades_fx[n] = t;
        n++;
    }

    fclose(fp);
    if (status < 0) {
        printf("%s: invalid row or read error after trade %lld\n", path, n);
        return NULL;
    }
    *n_out = n;
    return trades_fx;
}

/* free_trades_fx
 * What:    nothing to free: the array is global and lives for the whole run
 */
void free_trades_fx(TradeFx *trades) {
    (void)trades;
}
