/*
 * signal.h - BUY / SELL signals from a short/long SMA crossover
 *
 * BUY  when the short SMA moves from at-or-below the long SMA to above it.
 * SELL when the short SMA moves from at-or-above the long SMA to below it.
 * A crossover needs two consecutive trades where both SMAs exist.
 * The previous pair and the counts live inside signal.c (no structs or pointers).
 */
#ifndef SIGNAL_H
#define SIGNAL_H

#define SIG_BUY   1
#define SIG_SELL -1
#define SIG_NONE  0

void crossover_init(void);

/* Returns SIG_BUY, SIG_SELL or SIG_NONE for this trade. */
int  crossover_update(double sma_short, double sma_long);

int  crossover_buys(void);    /* total BUY signals so far  */
int  crossover_sells(void);   /* total SELL signals so far */

#endif /* SIGNAL_H */
