/*
 * sma.c - simple moving average (see sma.h)
 */
#include "sma.h"

/* sma_init
 * What:    empty buffer with the given window size
 * In/Out:  s = the Sma to set up, window = N. Returns nothing.
 * Watch:   reject window < 1 or window > MAX_WIN
 */
void sma_init(Sma *s, int window) {
    /* TODO */
    if (window < 1 || window > MAX_WIN){
        printf("SMA INIT FAILED: window size out of bounds");
    }else{
        s->head = 0;
        s->count = 0;
        s->window = window;
    }
}

/* sma_push
 * What:    add a price; once the buffer is full, drop the oldest one
 * In/Out:  s = the Sma to update, price = new price. Returns nothing.
 * Steps:   1) if count == window, subtract win[head] from sum (that is the oldest)
 *          2) win[head] = price, sum += price
 *          3) head = (head + 1) % window
 *          4) if count < window, count++
 */
void sma_push(Sma *s, double price) {
    /* TODO */
    if (s->count == s->window){
        s->sum -= s->win[s->head];
    }
    s->win[s->head] = price;
    s->sum += price;
    s->head = (s->head+1) % s->window; // if head+1 is 128. After mod, index 0
    if (s->count < s->window){
        s->count++;
    }  
}

/* sma_ready
 * What:    has the window filled up yet?
 * In/Out:  s = the Sma (read only). Returns 1 if count == window, else 0.
 */
int sma_ready(const Sma *s) {
    /* TODO */
    if (s->count == s->window){
        return 1;
    }
    return 0;
}

/* sma_value
 * What:    the average of the prices in the window = sum / window
 * In/Out:  s = the Sma (read only). Returns the average.
 * Watch:   only meaningful when sma_ready is 1; the caller decides what to print before that
 */
double sma_value(const Sma *s) {
    /* TODO */
    if (sma_ready(s)){
    double average = s->sum / s->window;
    return average;
    }
    return 0.0;
}
