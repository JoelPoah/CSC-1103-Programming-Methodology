/*
 * ohlc.c - one-second OHLC bars (see ohlc.h)
 */
#include "ohlc.h"

/* the bar being built right now */
static long long bar_sec = 0;
static double    bar_open = 0.0, bar_high = 0.0, bar_low = 0.0, bar_close = 0.0;
static int       bar_n = 0;
static int       bar_active = 0;   /* 1 once a bar has been started */

/* the last finished bar, ready to print */
static long long finished_sec = 0;
static double    finished_open = 0.0, finished_high = 0.0, finished_low = 0.0, finished_close = 0.0;
static int       finished_n = 0;

/* copy the current bar into the finished-bar variables */
static void finish_bar(void) {
    finished_sec = bar_sec;
    finished_open = bar_open;
    finished_high = bar_high;
    finished_low = bar_low;
    finished_close = bar_close;
    finished_n = bar_n;
}

/* ohlc_init
 * What:    mark that no bar has started yet
 * In/Out:  nothing. Returns nothing.
 * Watch:   the first trade must start a bar, not compare against garbage
 */
void ohlc_init(void) {
    bar_sec = 0;
    bar_open = 0.0;
    bar_high = 0.0;
    bar_low = 0.0;
    bar_close = 0.0;
    bar_n = 0;
    bar_active = 0;
}

/* ohlc_update
 * What:    put one trade into the current bar, or start a new bar
 * In/Out:  time_us = trade timestamp in microseconds, price = trade price
 * Returns: 1 if a bar was closed by this trade (copied to finished_*), else 0
 * Steps:   1) sec = time_us / MICROS_PER_SEC
 *          2) if no bar yet, or sec differs from bar_sec:
 *               - if a bar exists, copy it to finished_* (return 1)
 *               - start a new bar: open = high = low = close = price, n = 1
 *          3) otherwise: update high / low, close = price, n++
 * Watch:   the last bar is never closed here; the caller must call ohlc_flush
 */
int ohlc_update(long long time_us, double price) {
    long long current_second = time_us / MICROS_PER_SEC;
    int closed = 0;
    if ( !bar_active || current_second != bar_sec){
        if (bar_active){
            finish_bar();
            closed = 1;
        }
        bar_sec = current_second;
        bar_open = bar_high = bar_low = bar_close = price;
        bar_n = 1;
        bar_active = 1;
        return closed;
    }else{
        if(bar_high < price){
            bar_high = price;
        }else if (bar_low > price){
            bar_low = price;
        }
        bar_close = price;
        bar_n++;
    }
    return 0;
}

/* ohlc_flush
 * What:    finish the last bar after the final trade
 * Returns: 1 if a bar was active (copied to finished_*), else 0
 */
int ohlc_flush(void) {
    if (bar_active){
        finish_bar();
        bar_active = 0;
        return 1;
    }
    return 0;
}

long long ohlc_finished_sec(void)   { return finished_sec; }
double    ohlc_finished_open(void)  { return finished_open; }
double    ohlc_finished_high(void)  { return finished_high; }
double    ohlc_finished_low(void)   { return finished_low; }
double    ohlc_finished_close(void) { return finished_close; }
int       ohlc_finished_n(void)     { return finished_n; }
