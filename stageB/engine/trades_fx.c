/*
 * trades_fx.c - load all trades into memory as fixed-point (see trades_fx.h)
 */
#include <stdio.h>
#include "trades_fx.h"
#include "../../helper_function/trade_io.h"

/* Global for the same reason as trades[]: 320 MB is too big for the stack. */
TradeFx trades_fx[MAX_TRADES];

/* to_fixed
 * What:    turn a decimal into a scaled whole number, e.g. 76589.57 -> 7658957
 * Why +0.5: a double can't hold most decimals exactly, so 76589.57 x 100 may
 *          come out as 7658956.9999999. Casting to long long just chops off
 *          the fraction (giving 7658956, wrong). Adding 0.5 first rounds to
 *          the nearest whole number instead. Works because values are positive.
 */
static long long to_fixed(double value, long long scale) {
    return (long long)(value * scale + 0.5);
}

/* load_trades_fx
 * What:    copy every trade from the CSV into trades_fx[], converting to fixed-point
 * In/Out:  path = the CSV file. Returns the number of trades loaded, or -1
 *          if the file cannot be opened or has too many trades.
 * Note:    the helper library gives us doubles, so we convert once here.
 *          The engine then never touches a double until it prints.
 */
long long load_trades_fx(const char *path) {
    long long n = 0;

    if (!open_trades(path)) {
        printf("cannot open %s\n", path);
        return -1;
    }

    while (next_trade()) {
        if (n >= MAX_TRADES) {
            printf("too many trades in %s (limit is %d)\n", path, MAX_TRADES);
            close_trades();
            return -1;
        }
        trades_fx[n].time   = trade_time();
        trades_fx[n].price  = to_fixed(trade_price(), PRICE_SCALE);
        trades_fx[n].qty    = to_fixed(trade_qty(), QTY_SCALE);
        trades_fx[n].is_buy = trade_is_buy();
        n++;
    }

    close_trades();
    return n;
}
