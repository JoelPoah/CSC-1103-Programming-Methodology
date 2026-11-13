/*
 * stream_engine.h - Version 0 ("v0") for the Stage C replay and rival bot
 *
 * The group's Version 0 (ladder V0), driven one trade at a time. It adds no
 * new maths; it only calls the helper library and the Stage C engine
 * modules in this folder (copied from Stage B):
 *   - the helper library (helper_function/trade_io.c): next_trade() reads
 *     ONE line from the CSV file with fgets and splits it with sscanf
 *   - vwap.c, ohlc.c, sma.c + the NAIVE sma_naive.c (adds up the whole
 *     window again every trade) and signal.c, double prices
 *
 * One call of stream_step() does the same work as one pass of the naive
 * build of the trade loop in stageB/stageB_inmem.c, without the printing.
 */
#ifndef STREAM_ENGINE_H
#define STREAM_ENGINE_H

/* Open the file and reset every module.
 * Returns 1 if OK, 0 if the file cannot be opened. */
int stream_start(char path[], int short_win, int long_win);

/* Read and throw away the next count lines (used by --from-min).
 * The skipped trades are NOT processed. */
void stream_skip(long long count);

/* Read the next trade from the file and process it.
 * Returns SIG_BUY (1), SIG_SELL (-1) or SIG_NONE (0). */
int stream_step(void);

/* Price of the trade just processed, in cents (for the rival bot P&L). */
long long stream_price_cents(void);

/* Close the file. */
void stream_stop(void);

/* Results so far. */
int    stream_buys(void);
int    stream_sells(void);
double stream_vwap(void);

#endif /* STREAM_ENGINE_H */
