/*
 * ohlc.c - one-second OHLC bars (see ohlc.h)
 */
#include "ohlc.h"

/* ohlc_init
 * What:    mark that no bar has started yet
 * In/Out:  bar = the OhlcBar to reset. Returns nothing.
 * Watch:   the first trade must start a bar, not compare against garbage
 */
void ohlc_init(OhlcBar *bar) {
    /* TODO not very sure do we init all to 0 first? */
    bar->sec =0;
    bar->open = 0.0;
    bar->high =0.0;
    bar->low =0.0;
    bar->close=0.0;
    bar->n=0;
    bar->active = 0;
}

/* ohlc_update
 * What:    put one trade into the current bar, or start a new bar
 * In/Out:  bar      = the current bar (updated in place)
 *          time_us  = trade timestamp in microseconds
 *          price    = trade price
 *          finished = filled with the old bar if one just closed
 * Returns: 1 if a bar was closed by this trade (copy it to *finished), else 0
 * Steps:   1) sec = time_us / MICROS_PER_SEC
 *          2) if no bar yet, or sec differs from bar->sec:
 *               - if a bar exists, copy it to *finished and return 1
 *               - start a new bar: open = high = low = close = price, n = 1
 *          3) otherwise: update high / low, close = price, n++
 * Watch:   the last bar is never closed here; the caller must flush it
 */
int ohlc_update(OhlcBar *bar, long long time_us, double price, OhlcBar *finished) {
    /* TODO */
    long long current_second = time_us / MICROS_PER_SEC;
    if (bar->active==0 || current_second != bar->sec){
        bar->open = bar->high = bar->low = bar->close = price;
        bar->n = 1;
    }else if (current_second != bar->sec){
        finished = bar;
        return 1;
    }else{
        if(bar->high < price){
            bar->high = price;
        }else if (bar->low > price){
            bar->low = price; 
        }
        bar->close = price;
    return 0;
    }

}
