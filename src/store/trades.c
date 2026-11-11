/*
 * trades.c - load all trades into memory (see trades.h)
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
 *          or -1 if the file cannot be opened or has too many trades.
 * Steps:   1) open the file
 *          2) read_trade writes straight into the next free struct:
 *             &trades[n].price is the address of that struct's price
 *          3) close the file and return n
 * Watch:   never write past the end of the array (n must stay < MAX_TRADES)
 */
long long load_trades(const char *path) {
    long long n = 0;
    FILE *trades_file = fopen(path, "r");

    if (trades_file == NULL) {
        printf("cannot open %s\n", path);
        return -1;
    }

    while (n < MAX_TRADES &&
           read_trade(trades_file, &trades[n].time, &trades[n].price,
                      &trades[n].qty, &trades[n].is_buy)) {
        n++;
    }

    /* the array is full: is there still a trade left in the file? */
    if (n == MAX_TRADES) {
        Trade extra;
        if (read_trade(trades_file, &extra.time, &extra.price, &extra.qty, &extra.is_buy)) {
            printf("too many trades in %s (limit is %d)\n", path, MAX_TRADES);
            fclose(trades_file);
            return -1;
        }
    }

    fclose(trades_file);
    return n;
}
