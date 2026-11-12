/*
 * v3-v4_trades.c - load all trades into memory (Leroi, see trades.h)  (V3, V4)
 *
 * Same as stageB/engine/trades.c, except each trade is read with the
 * Stage A+ reader read_trade() (link v2-v4_read_trade_fast.c) instead of the
 * helper library, so the in-memory versions keep the V2 reader.
 */
#include <stdio.h>
#include "trades.h"
#include "../reader/read_trade.h"

/* The actual global array. A global (not a local inside main) because
 * 10 million x 32 bytes = 320 MB, which is far too big for the stack. */
Trade trades[MAX_TRADES];

/* load_trades
 * What:    copy every trade from the CSV file into trades[]
 * In/Out:  path = the CSV file. Returns the number of trades loaded,
 *          or -1 if the file cannot be opened, has a bad row, or has too many trades.
 * Steps:   1) open the file
 *          2) for each trade, fill in trades[n] and add 1 to n
 *          3) close the file and return n
 * Watch:   never write past the end of the array (n must stay < MAX_TRADES)
 */
long long load_trades(const char *path) {
    long long n = 0;
    long long time;
    double price, qty;
    int is_buy, status;
    FILE *fp = fopen(path, "r");

    if (fp == NULL) {
        printf("cannot open %s\n", path);
        return -1;
    }

    while ((status = read_trade(fp, &time, &price, &qty, &is_buy)) == 1) {
        if (n >= MAX_TRADES) {
            /* stop instead of writing outside the array */
            printf("too many trades in %s (limit is %d)\n", path, MAX_TRADES);
            fclose(fp);
            return -1;
        }
        trades[n].time   = time;
        trades[n].price  = price;
        trades[n].qty    = qty;
        trades[n].is_buy = is_buy;
        n++;
    }

    fclose(fp);
    if (status < 0) {
        printf("%s: invalid row or read error after trade %lld\n", path, n);
        return -1;
    }
    return n;
}
