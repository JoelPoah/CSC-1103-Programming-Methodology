/*
 * v0_engine.h - Version 0 for the Stage C replay and rival bot
 *
 * Version 0 is the group's Stage A program (stageA/stageA_manual.c).
 * This file adds no new maths. It only calls the code from Stage A:
 *   - the helper library (helper_function/trade_io.c): next_trade() reads
 *     ONE line from the CSV file with fgets and splits it with sscanf
 *   - the Stage A engine modules (stageA/engine/): vwap.c, ohlc.c,
 *     the NAIVE sma.c (adds up the whole window again every trade) and signal.c
 *
 * One call of v0_step() does the same work as one pass of the while loop
 * in stageA_manual.c (without the printing).
 */
#ifndef V0_ENGINE_H
#define V0_ENGINE_H

/* Open the file and reset every Stage A module.
 * Returns 1 if OK, 0 if the file cannot be opened. */
int v0_start(char path[], int short_win, int long_win);

/* Read and throw away the next count lines (used by --from-min).
 * The skipped trades are NOT processed. */
void v0_skip(long long count);

/* Read the next trade from the file and process it.
 * Returns SIG_BUY (1), SIG_SELL (-1) or SIG_NONE (0). */
int v0_step(void);

/* Price of the trade just processed, in cents (for the rival bot P&L). */
long long v0_price_cents(void);

/* Close the file. */
void v0_stop(void);

/* Results so far. */
int    v0_buys(void);
int    v0_sells(void);
double v0_vwap(void);

#endif /* V0_ENGINE_H */
