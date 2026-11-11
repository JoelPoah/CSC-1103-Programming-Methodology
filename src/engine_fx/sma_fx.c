/*
 * sma_fx.c - running-sum SMA with fixed-point prices (see sma_fx.h)
 */
#include "sma_fx.h"
#include "fixed.h"

/* one row per window: win[SHORT_SMA_FX][...] and win[LONG_SMA_FX][...] */
static long long win[SMA_FX_COUNT][MAX_WIN_FX];   /* circular array of the most recent prices */
static int       window[SMA_FX_COUNT];            /* N: how many prices the average covers     */
static int       head[SMA_FX_COUNT];              /* next slot to overwrite                    */
static int       count[SMA_FX_COUNT];             /* prices seen so far, capped at window      */
static long long sum[SMA_FX_COUNT];               /* sum of the prices currently in the array  */

/* sma_fx_init
 * What:    empty window with the given size
 * In/Out:  id = SHORT_SMA_FX or LONG_SMA_FX, size = N. Returns nothing.
 * Watch:   a size outside 1..MAX_WIN_FX falls back to 1 so later calls
 *          stay safe (the main program checks the sizes and prints the error)
 */
void sma_fx_init(int id, int size) {
    head[id] = 0;
    count[id] = 0;
    sum[id] = 0;
    if (size < 1 || size > MAX_WIN_FX){
        window[id] = 1;
    }else{
        window[id] = size;
    }
}

/* sma_fx_push
 * What:    add a price; once the window is full, drop the oldest one
 * In/Out:  id = which window, price = new scaled price. Returns nothing.
 * Steps:   1) if count == window, subtract win[head] from sum (that is the oldest)
 *          2) win[head] = price, sum += price
 *          3) head = (head + 1) % window
 *          4) if count < window, count++
 */
void sma_fx_push(int id, long long price) {
    if (count[id] == window[id]){
        sum[id] -= win[id][head[id]];
    }
    win[id][head[id]] = price;
    sum[id] += price;
    head[id] = (head[id] + 1) % window[id]; // if head+1 is window. After mod, index 0
    if (count[id] < window[id]){
        count[id]++;
    }
}

/* sma_fx_ready
 * What:    has the window filled up yet?
 * In/Out:  id = which window. Returns 1 if count == window, else 0.
 */
int sma_fx_ready(int id) {
    if (count[id] == window[id]){
        return 1;
    }
    return 0;
}

/* sma_fx_sum
 * What:    the exact window sum. The crossover test uses this directly,
 *          so the main loop never has to divide.
 */
long long sma_fx_sum(int id) {
    return sum[id];
}

/* sma_fx_value
 * What:    the average in USDT = sum / window / PRICE_SCALE, only for printing
 * Watch:   returns 0.0 until the window is full
 */
double sma_fx_value(int id) {
    if (sma_fx_ready(id)){
        double average = (double)sum[id] / window[id] / PRICE_SCALE;
        return average;
    }
    return 0.0;
}
