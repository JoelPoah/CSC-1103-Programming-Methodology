/*
 * ohlc_fx.c - one-second OHLC bars with fixed-point prices (see ohlc_fx.h)
 *
 * Stage C: copied from stageB/engine/ohlc_fx.c (unchanged), the same way Stage B
 * copied Stage A's modules, so Stage C's engine and its assembly live here.
 */
#include "ohlc_fx.h"

/* ohlc_fx_init
 * What:    mark that no bar has started yet
 */
void ohlc_fx_init(OhlcBarFx *bar) {
    bar->sec = 0;
    bar->open = bar->high = bar->low = bar->close = 0;
    bar->n = 0;
    bar->active = 0;
}

/* ohlc_fx_update
 * What:    put one trade into the current bar, or start a new bar
 * Steps:   1) sec = time_us / MICROS_PER_SEC (integer division drops the micros)
 *          2) if no bar yet, or the second changed:
 *               - if a bar exists, copy it to *finished (return 1)
 *               - start a new bar with this price
 *          3) otherwise update high / low / close and the count
 * Watch:   the last bar is never closed here; the caller prints it at the end
 */
int ohlc_fx_update(OhlcBarFx *bar, long long time_us, long long price,
                   OhlcBarFx *finished) {
    long long sec = time_us / MICROS_PER_SEC;
    int closed = 0;

    if (!bar->active || sec != bar->sec) {
        if (bar->active) {
            *finished = *bar;
            closed = 1;
        }
        bar->sec = sec;
        bar->open = bar->high = bar->low = bar->close = price;
        bar->n = 1;
        bar->active = 1;
        return closed;
    }

    if (price > bar->high) bar->high = price;
    if (price < bar->low)  bar->low = price;
    bar->close = price;
    bar->n++;
    return 0;
}
