/*
 * ohlc.h - one-second OHLC bars (open, high, low, close)
 *
 * Trades are grouped by second (time / 1,000,000 microseconds).
 * When a trade arrives in a new second, the previous bar is finished
 * and can be printed.
 *
 * The current bar is four variables (+ second and trade count) inside
 * ohlc.c. A finished bar is copied to a second set of variables, read
 * back with the ohlc_finished_* functions. No structs or pointers.
 */
#ifndef OHLC_H
#define OHLC_H

#define MICROS_PER_SEC 1000000LL   /* trade time is in microseconds */

void ohlc_init(void);

/* Add one trade. Returns 1 if the previous bar was finished by this trade
 * (read it with the ohlc_finished_* functions), otherwise 0. */
int  ohlc_update(long long time_us, double price);

/* After the last trade: finish the current bar. Returns 1 if there was one
 * (read it with the ohlc_finished_* functions), otherwise 0. */
int  ohlc_flush(void);

/* the most recently finished bar */
long long ohlc_finished_sec(void);
double    ohlc_finished_open(void);
double    ohlc_finished_high(void);
double    ohlc_finished_low(void);
double    ohlc_finished_close(void);
int       ohlc_finished_n(void);

#endif /* OHLC_H */
