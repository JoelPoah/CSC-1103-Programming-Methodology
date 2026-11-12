/*
 * sma.h - simple moving average over the last N prices
 *
 * Uses an array as a circular window and a running sum, so each new price
 * costs O(1). There are two windows, picked by an id: SHORT_SMA or LONG_SMA.
 * The arrays live inside sma.c, so no structs or pointers are needed.
 */
#ifndef SMA_H
#define SMA_H

#define MAX_WIN 128            /* largest window the array can hold */

#define SHORT_SMA 0            /* id of the short window */
#define LONG_SMA  1            /* id of the long window  */
#define SMA_COUNT 2            /* how many windows exist */

void   sma_init(int id, int window);   /* window must be 1..MAX_WIN */
void   sma_push(int id, double price);
int    sma_ready(int id);              /* 1 once count == window    */
double sma_value(int id);

#endif /* SMA_H */
