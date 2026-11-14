/*
 * fixed_engine.h - the fastest version ("mem") for the Stage C replay and rival bot
 *
 * The same version as ladder V7 (bin/v7_stageC_fread), driven one trade at a
 * time. It adds no new maths; it only calls the Stage C engine modules in
 * this folder:
 *   - trades_fx_fread.c: load_trades_fx() reads the whole file with one
 *     fread into a malloc'd array of TradeFx, parsed straight to fixed-point
 *     (cents) by read_trade_fx.c (Stage C's extension of Stage B's array).
 *     The replay loads this array ONCE before it starts and hands this
 *     engine one TradeFx at a time, so it never reads the file during the replay.
 *   - vwap_fx.c, ohlc_fx.c, the running-sum sma_fx.c and signal_fx.c,
 *     copied from Stage B. Prices are whole numbers of cents, so every
 *     signal is exact.
 *
 * One call of fixed_step() does the same work as one pass of the trade loop
 * in stageB/stageB_fixed.c (without the printing).
 */
#ifndef FIXED_ENGINE_H
#define FIXED_ENGINE_H

#include "trades_fx.h"     /* TradeFx, load_trades_fx(), free_trades_fx() */

/* Reset every fixed-point module. */
void fixed_start(int short_win, int long_win);

/* Process one trade from the TradeFx array.
 * Returns SIG_BUY (1), SIG_SELL (-1) or SIG_NONE (0). */
int fixed_step(const TradeFx *t);

/* A trade's price in cents (for the rival bot P&L). */
long long fixed_price_cents(const TradeFx *t);

/* Results so far. */
int    fixed_buys(void);
int    fixed_sells(void);
double fixed_vwap(void);

#endif /* FIXED_ENGINE_H */
