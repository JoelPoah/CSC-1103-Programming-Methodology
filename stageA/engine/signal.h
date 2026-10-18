/*
 * signal.h - BUY / SELL signals from a short/long SMA crossover
 *
 * BUY  when the short SMA moves from at-or-below the long SMA to above it.
 * SELL when the short SMA moves from at-or-above the long SMA to below it.
 * A crossover needs two consecutive trades where both SMAs exist.
 */
#ifndef SIGNAL_H
#define SIGNAL_H

#define SIG_BUY   1
#define SIG_SELL -1
#define SIG_NONE  0

typedef struct {
    double prev_short;   /* short SMA on the previous trade */
    double prev_long;    /* long SMA on the previous trade  */
    int    have_prev;    /* 1 once a previous pair exists   */
    int    buys;         /* total BUY signals so far        */
    int    sells;        /* total SELL signals so far       */
} Crossover;

void        crossover_init(Crossover *x);

/* Returns SIG_BUY, SIG_SELL or SIG_NONE for this trade. */
int         crossover_update(Crossover *x, double sma_short, double sma_long);

/* "BUY", "SELL" or "" - used when printing the table. */
const char *signal_name(int sig);

#endif /* SIGNAL_H */
