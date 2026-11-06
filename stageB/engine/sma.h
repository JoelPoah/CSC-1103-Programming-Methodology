/*
 * sma.h - simple moving average over the last N prices
 *
 * Uses a circular buffer and a running sum, so each new price costs O(1).
 * The same struct is used for the short and the long window.
 */
#ifndef SMA_H
#define SMA_H

#define MAX_WIN 128            /* largest window the buffer can hold */

typedef struct {
    double win[MAX_WIN];   /* circular buffer of the most recent prices */
    int    window;         /* N: how many prices the average covers     */
    int    head;           /* next slot to overwrite                    */
    int    count;          /* prices seen so far, capped at window      */
    double sum;            /* sum of the prices currently in the buffer */
} Sma;

void   sma_init(Sma *s, int window);   /* window must be 1..MAX_WIN */
void   sma_push(Sma *s, double price);
int    sma_ready(const Sma *s);        /* 1 once count == window    */
double sma_value(const Sma *s);

#endif /* SMA_H */
