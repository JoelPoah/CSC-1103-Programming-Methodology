/*
 * mem_engine.h - the fastest version ("mem") for the Stage C replay and rival bot
 *
 * This file adds no new maths. It only calls the group's earlier code:
 *   - Leroi's Stage C loader (stageC/engine/trades_malloc.c):
 *     load_trades_malloc() reads the whole file ONCE into an array made
 *     with malloc to fit the file. The replay hands this engine one Trade
 *     from that array at a time, so it never reads the file during the replay.
 *   - Leroi's Stage B fixed-point modules (stageB/engine/):
 *     vwap_fx.c, ohlc_fx.c, the running-sum sma_fx.c and signal_fx.c.
 *     Prices are whole numbers of cents, so every signal is exact.
 */
#ifndef MEM_ENGINE_H
#define MEM_ENGINE_H

#include "trades_malloc.h"     /* the Trade struct and load_trades_malloc() */

/* Reset every fixed-point module. */
void mem_start(int short_win, int long_win);

/* Process one trade from Leroi's array.
 * Returns SIG_BUY (1), SIG_SELL (-1) or SIG_NONE (0). */
int mem_step(Trade *t);

/* A trade's price in cents (for the rival bot P&L). */
long long mem_price_cents(Trade *t);

/* Results so far. */
int    mem_buys(void);
int    mem_sells(void);
double mem_vwap(void);

#endif /* MEM_ENGINE_H */
