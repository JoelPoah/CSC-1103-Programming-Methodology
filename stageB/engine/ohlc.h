/*
 * ohlc.h - one-second OHLC bars (open, high, low, close)
 *
 * Trades are grouped by second (time / 1,000,000 microseconds).
 * When a trade arrives in a new second, the previous bar is finished
 * and can be printed.
 */
#ifndef OHLC_H
#define OHLC_H

#define MICROS_PER_SEC 1000000LL   /* trade time is in microseconds */

typedef struct {
    long long sec;     /* which second this bar covers    */
    double    open;    /* first price in the second       */
    double    high;    /* highest price in the second     */
    double    low;     /* lowest price in the second      */
    double    close;   /* last price in the second        */
    int       n;       /* number of trades in the second  */
    int       active;  /* 1 once a bar has been started   */
} OhlcBar;

void ohlc_init(OhlcBar *bar);

/* Add one trade. Returns 1 if the previous bar was finished by this trade
 * (its values are copied into *finished), otherwise 0. */
int  ohlc_update(OhlcBar *bar, long long time_us, double price, OhlcBar *finished);

#endif /* OHLC_H */
