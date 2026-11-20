/*
 * sma.c - naive simple moving average (see sma.h)
 * Re-adds the whole window on every sma_value call: O(N) per trade.
 */
#include "sma.h"
#include <stdio.h>

/* one row per window: win[SHORT_SMA][...] and win[LONG_SMA][...] */
static double win[SMA_COUNT][MAX_WIN];   /* circular array of the most recent prices */
static int    window[SMA_COUNT];         /* N: how many prices the average covers     */
static int    head[SMA_COUNT];           /* next slot to overwrite                    */
static int    count[SMA_COUNT];          /* prices seen so far, capped at window      */

/* sma_init
 * What:    empty window with the given size
 * In/Out:  id = SHORT_SMA or LONG_SMA, size = N. Returns nothing.
 * Watch:   reject size < 1 or size > MAX_WIN (falls back to 1 so later calls stay safe)
 */
void sma_init(int id, int size) {
    head[id] = 0;
    count[id] = 0;
    if (size < 1 || size > MAX_WIN){
        printf("SMA INIT FAILED: window size out of bounds\n");
        window[id] = 1;
    }else{
        window[id] = size;
    }
}

/* sma_push
 * What:    add a price; once the window is full, it overwrites the oldest one
 * In/Out:  id = which window, price = new price. Returns nothing.
 * Steps:   1) win[head] = price (no running sum is kept)
 *          2) head = (head + 1) % window
 *          3) if count < window, count++
 */
void sma_push(int id, double price) {
    win[id][head[id]] = price;
    head[id] = (head[id] + 1) % window[id]; // if head+1 is window. After mod, index 0
    if (count[id] < window[id]){
        count[id]++;
    }
}

/* sma_ready
 * What:    has the window filled up yet?
 * In/Out:  id = which window. Returns 1 if count == window, else 0.
 */
int sma_ready(int id) {
    if (count[id] == window[id]){
        return 1;
    }
    return 0;
}

/* sma_value
 * What:    the average of the prices in the window, re-adding all N of them
 * In/Out:  id = which window. Returns the average.
 * Cost:    N additions on every call -> the slow part we measure on the ladder
 * Watch:   only meaningful when sma_ready is 1; the caller decides what to print before that
 */
double sma_value(int id) {
    if (sma_ready(id)){
        double total = 0.0;
        for (int i = 0; i < window[id]; i++){
            total += win[id][i];   /* order doesn't matter for a sum */
        }
        return total / window[id];
    }
    return 0.0;
}
