/*
 * engine.c - the three engine versions (see engine.h)
 */
#define __USE_MINGW_ANSI_STDIO 1     /* Windows: makes %lld work reliably */
#include <stdio.h>
#include <stdlib.h>                  /* malloc, free */
#include <string.h>                  /* strcmp */
#include "engine.h"
#include "fast_parse.h"
#include "../../stageB/engine/sma_naive.h"

/* parse_line_v0
 * What:    Version 0's parser: exactly the sscanf call in trade_io.c
 * Returns: 1 if the line is a trade, 0 if not
 */
static int parse_line_v0(const char *line, long long *time_us, double *price,
                         double *qty) {
    long long id;
    double quote;
    char maker[16], best[16];

    return sscanf(line, "%lld,%lf,%lf,%lf,%lld,%15[^,],%15s",
                  &id, price, qty, &quote, time_us, maker, best) == 7;
}

/* step_v0
 * What:    Version 0 for one trade: sscanf -> VWAP -> OHLC -> naive SMA -> crossover
 * Note:    the OHLC bar is updated (it is part of the work) but not printed
 */
static int step_v0(Engine *e, const char *line, long long *price_cents) {
    long long time_us;
    double price, qty;
    int sig = SIG_NONE;

    if (!parse_line_v0(line, &time_us, &price, &qty)) {
        *price_cents = 0;
        return SIG_NONE;
    }
    *price_cents = (long long)(price * PRICE_SCALE + 0.5);

    vwap_update(&e->vwap, price, qty);
    ohlc_update(&e->bar, time_us, price, &e->finished);

    /* naive SMA: store the price, then re-add the whole window */
    sma_naive_push(&e->sma_short, price);
    sma_naive_push(&e->sma_long, price);
    if (sma_ready(&e->sma_short) && sma_ready(&e->sma_long)) {
        sig = crossover_update(&e->cross, sma_naive_value(&e->sma_short),
                               sma_naive_value(&e->sma_long));
    }
    return sig;
}

/* step_fx
 * What:    the fixed-point engine for one already-parsed trade
 *          (shared by FAST and MEM, so they always give the same signals)
 * Steps:   VWAP sums -> OHLC -> running-sum SMAs -> exact integer crossover
 */
static int step_fx(Engine *e, const TradeFx *t) {
    vwap_fx_update(&e->vwap_fx, t->price, t->qty);
    ohlc_fx_update(&e->bar_fx, t->time, t->price, &e->finished_fx);

    sma_fx_push(&e->sma_short_fx, t->price);
    sma_fx_push(&e->sma_long_fx, t->price);
    if (sma_fx_ready(&e->sma_short_fx) && sma_fx_ready(&e->sma_long_fx)) {
        return crossover_fx_update(&e->cross_fx,
                                   sma_fx_sum(&e->sma_short_fx), e->short_win,
                                   sma_fx_sum(&e->sma_long_fx), e->long_win);
    }
    return SIG_NONE;
}

/* engine_init
 * What:    reset every indicator; for MEM, parse the whole feed once
 * Watch:   MEM's array is malloc'd to fit the feed; engine_free releases it
 */
int engine_init(Engine *e, EngineKind kind, int short_win, int long_win,
                const Feed *feed) {
    long long i;

    e->kind = kind;
    e->short_win = short_win;
    e->long_win = long_win;
    e->parsed = NULL;

    vwap_init(&e->vwap);
    ohlc_init(&e->bar);
    ohlc_init(&e->finished);
    sma_init(&e->sma_short, short_win);
    sma_init(&e->sma_long, long_win);
    crossover_init(&e->cross);

    vwap_fx_init(&e->vwap_fx);
    ohlc_fx_init(&e->bar_fx);
    ohlc_fx_init(&e->finished_fx);
    sma_fx_init(&e->sma_short_fx, short_win);
    sma_fx_init(&e->sma_long_fx, long_win);
    crossover_fx_init(&e->cross_fx);

    if (kind == ENGINE_MEM) {
        e->parsed = malloc((size_t)feed->n * sizeof(TradeFx));
        if (e->parsed == NULL) {
            printf("engine: malloc failed for %lld trades\n", feed->n);
            return 0;
        }
        for (i = 0; i < feed->n; i++) {
            if (!parse_trade_fx(feed->line[i], &e->parsed[i])) {
                printf("engine: line %lld is not a trade\n", i + 1);
                engine_free(e);
                return 0;
            }
        }
    }
    return 1;
}

/* engine_step
 * What:    process trade i with the chosen version
 * Returns: SIG_BUY, SIG_SELL or SIG_NONE; *price_cents = this trade's price
 */
int engine_step(Engine *e, const Feed *feed, long long i, long long *price_cents) {
    TradeFx t;

    switch (e->kind) {
    case ENGINE_V0:
        return step_v0(e, feed->line[i], price_cents);

    case ENGINE_FAST:
        if (!parse_trade_fx(feed->line[i], &t)) {
            *price_cents = 0;
            return SIG_NONE;
        }
        *price_cents = t.price;
        return step_fx(e, &t);

    case ENGINE_MEM:
    default:
        *price_cents = e->parsed[i].price;
        return step_fx(e, &e->parsed[i]);
    }
}

/* engine_free
 * What:    release the MEM array (no-op for the other versions)
 */
void engine_free(Engine *e) {
    free(e->parsed);
    e->parsed = NULL;
}

int engine_buys(const Engine *e) {
    return e->kind == ENGINE_V0 ? e->cross.buys : e->cross_fx.buys;
}

int engine_sells(const Engine *e) {
    return e->kind == ENGINE_V0 ? e->cross.sells : e->cross_fx.sells;
}

double engine_vwap(const Engine *e) {
    return e->kind == ENGINE_V0 ? vwap_value(&e->vwap) : vwap_fx_value(&e->vwap_fx);
}

const char *engine_name(EngineKind kind) {
    switch (kind) {
    case ENGINE_V0:   return "v0";
    case ENGINE_FAST: return "fast";
    default:          return "mem";
    }
}

int engine_parse_kind(const char *name, EngineKind *kind) {
    if (strcmp(name, "v0") == 0)   { *kind = ENGINE_V0;   return 1; }
    if (strcmp(name, "fast") == 0) { *kind = ENGINE_FAST; return 1; }
    if (strcmp(name, "mem") == 0)  { *kind = ENGINE_MEM;  return 1; }
    return 0;
}
