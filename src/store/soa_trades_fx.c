/*
 * soa_trades_fx.c - fixed-point trades in four global arrays (Leroi, stretch goal)
 *
 * Same as v5_trades_fx_array.c, except each field of a trade goes into its
 * own array instead of one TradeFx struct (see trades_soa.h for why).
 */
#include <stdio.h>
#include "trades_soa.h"
#include "trades_fx.h"               /* TradeFx, MAX_TRADES */
#include "../reader/read_trade_fx.h"

/* Global for the same reason as V5's array: 3 x 80 MB + 40 MB = 280 MB is far
 * too big for the stack. Static, so only this file can touch them; main gets
 * pointers to them through TradesSoa instead. */
static long long soa_time[MAX_TRADES];
static long long soa_price[MAX_TRADES];
static long long soa_qty[MAX_TRADES];
static int       soa_is_buy[MAX_TRADES];

/* load_trades_soa
 * What:    copy every trade from the CSV into the four arrays as fixed-point
 * In/Out:  path = the CSV file, *out = pointers to the arrays and the count.
 *          Returns 1 on success, or 0 if the file cannot be opened, has a
 *          bad row, or has too many trades.
 * Watch:   never write past the end of the arrays (n must stay < MAX_TRADES)
 */
int load_trades_soa(const char *path, TradesSoa *out) {
    long long n = 0;
    int status;
    TradeFx t;              /* Lucas's reader fills one struct; we split it up */
    FILE *fp = fopen(path, "r");

    out->n = 0;
    if (fp == NULL) {
        printf("cannot open %s\n", path);
        return 0;
    }

    while ((status = read_trade_fx(fp, &t)) == 1) {
        if (n >= MAX_TRADES) {
            /* stop instead of writing outside the arrays */
            printf("too many trades in %s (limit is %d)\n", path, MAX_TRADES);
            fclose(fp);
            return 0;
        }
        soa_time[n]   = t.time;
        soa_price[n]  = t.price;
        soa_qty[n]    = t.qty;
        soa_is_buy[n] = t.is_buy;
        n++;
    }

    fclose(fp);
    if (status < 0) {
        printf("%s: invalid row or read error after trade %lld\n", path, n);
        return 0;
    }

    out->time   = soa_time;
    out->price  = soa_price;
    out->qty    = soa_qty;
    out->is_buy = soa_is_buy;
    out->n      = n;
    return 1;
}
