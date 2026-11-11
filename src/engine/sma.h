/*
 * sma.h - simple moving average over the last N prices
 *
 * One header, two versions of the code. The Makefile links exactly one:
 *   sma_naive.c   - re-adds the whole window on every trade: O(N) per trade
 *   sma_running.c - keeps a running sum, one add + one subtract: O(1) per trade
 * Both use an array as a circular window of the last N prices and have the
 * same function names, so the main program does not change at all.
 * There are two windows, picked by an id: SHORT_SMA or LONG_SMA.
 * The arrays live inside the .c file, so no structs or pointers are needed.
 */
#ifndef SMA_H
#define SMA_H

#define MAX_WIN 256            /* largest window the array can hold */

#define SHORT_SMA 0            /* id of the short window */
#define LONG_SMA  1            /* id of the long window  */
#define SMA_COUNT 2            /* how many windows exist */

void   sma_init(int id, int window);   /* window must be 1..MAX_WIN */
void   sma_push(int id, double price);
int    sma_ready(int id);              /* 1 once count == window    */
double sma_value(int id);

#endif /* SMA_H */
