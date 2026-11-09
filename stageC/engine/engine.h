/*
 * engine.h - one trading engine, three versions behind one interface (Stage C)
 *
 * The replay and the rival bot must run Version 0 and the fastest version on
 * exactly the same data, so both are wrapped behind engine_step():
 *
 *   ENGINE_V0    Version 0: parse the line with sscanf, double prices,
 *                NAIVE SMA (re-add the whole window every trade).
 *                Same algorithm as stageA/stageA_manual.c.
 *   ENGINE_FAST  parse the line with parse_trade_fx() (Stage A+ style parser),
 *                fixed-point prices, running-sum SMA, exact integer crossover.
 *   ENGINE_MEM   fastest version: every line is parsed ONCE at load time into
 *                a malloc'd TradeFx array; each step only does the fixed-point
 *                maths (same as stageB/stageB_fixed.c's process phase).
 *
 * The engine modules themselves are the Stage B ones (stageB/engine/).
 */
#ifndef ENGINE_H
#define ENGINE_H

#include "feed.h"
#include "../../stageB/engine/vwap.h"
#include "../../stageB/engine/ohlc.h"
#include "../../stageB/engine/sma.h"
#include "../../stageB/engine/signal.h"
#include "../../stageB/engine/vwap_fx.h"
#include "../../stageB/engine/ohlc_fx.h"
#include "../../stageB/engine/sma_fx.h"
#include "../../stageB/engine/signal_fx.h"
#include "../../stageB/engine/trades_fx.h"

typedef enum { ENGINE_V0, ENGINE_FAST, ENGINE_MEM } EngineKind;

typedef struct {
    EngineKind kind;
    int        short_win, long_win;

    /* Version 0 state (double) */
    Vwap        vwap;
    OhlcBar     bar, finished;
    Sma         sma_short, sma_long;
    Crossover   cross;

    /* fixed-point state (FAST and MEM) */
    VwapFx      vwap_fx;
    OhlcBarFx   bar_fx, finished_fx;
    SmaFx       sma_short_fx, sma_long_fx;
    CrossoverFx cross_fx;

    /* MEM only: all trades parsed at load time (malloc'd, n entries) */
    TradeFx    *parsed;
} Engine;

/* Set up the engine. For ENGINE_MEM this also parses the whole feed into a
 * malloc'd array (the "load" phase, not timed per trade).
 * Returns 1 on success, 0 on error. */
int  engine_init(Engine *e, EngineKind kind, int short_win, int long_win,
                 const Feed *feed);

/* Process trade i of the feed. Returns SIG_BUY, SIG_SELL or SIG_NONE.
 * *price_cents receives this trade's price in cents (for the P&L). */
int  engine_step(Engine *e, const Feed *feed, long long i, long long *price_cents);

/* Free what engine_init allocated (only MEM allocates). */
void engine_free(Engine *e);

int  engine_buys(const Engine *e);
int  engine_sells(const Engine *e);
double engine_vwap(const Engine *e);

/* "v0", "fast", "mem" <-> EngineKind. parse returns 1 if the name is known. */
const char *engine_name(EngineKind kind);
int  engine_parse_kind(const char *name, EngineKind *kind);

#endif /* ENGINE_H */
