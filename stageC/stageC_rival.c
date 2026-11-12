/*
 * stageC_rival.c - "slow code loses money": rival bot P&L and the R sweep
 *
 * Steps:
 *   1) load the file into memory (feed.c)
 *   2) for each engine version (v0, fast, mem): run every trade through
 *      engine_step() as fast as possible and time EACH trade with now_ns().
 *      That per-trade processing time is our reaction time (brief, rival 2).
 *   3) the signals: the brief says both versions raise exactly the same
 *      signals. Ours do not quite: Version 0 uses double + naive SMA, and on
 *      trades where short SMA == long SMA, rounding gives d a random tiny
 *      sign (see docs/leroi_stageB_C.md, section 2). So the trading signals
 *      for EVERY version are the exact fixed-point ones (the correct answer),
 *      and each version only contributes its own reaction times. The P&L
 *      difference is then purely from speed, as the brief intends. The
 *      program also prints how many signals Version 0's own engine got
 *      different, for the report.
 *   4) for R = 1 us: P&L of every version and money lost = P&L(fastest) - P&L(V0)
 *   5) sweep R from 0.1 us to 100 us (10 steps per decade) and write
 *      stageC/rival_sweep.dat for gnuplot (stageC/plot_rival.gnuplot)
 *
 * Usage (from the project root):
 *   stageC_rival <file.csv> [--short S] [--long L] [--out file.dat]
 * Example:
 *   stageC_rival data/stress_full.csv
 *   gnuplot stageC/plot_rival.gnuplot        -> stageC/rival_sweep.png
 * On the Pi: sudo taskset -c 2 ./stageC/stageC_rival data/stress_full.csv
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* malloc, free, atoi */
#include <string.h>                      /* strcmp */
#include <math.h>                        /* pow */
#include "engine/feed.h"
#include "engine/engine.h"
#include "engine/rival.h"
#include "engine/latency.h"

#define N_ENGINES   3
#define R_MIN_US    0.1
#define R_DECADES   3           /* 0.1 -> 100 us */
#define R_PER_DEC   10          /* points per decade */
#define STANDARD_R  1.0         /* brief: standard rival = 1 us */

/* everything measured for one engine version */
typedef struct {
    EngineKind   kind;
    uint32_t    *react_ns;    /* processing time of every trade, ns   */
    signed char *sig;         /* that engine's own signal per trade   */
    LatStats     stats;
    int          buys, sells;
    double       vwap;
    double       total_ms;    /* whole pass, ms                       */
} Run;

/* measure
 * What:    run one engine over the whole feed, timing every trade
 * Steps:   engine_init (MEM parses everything here, untimed), then for each
 *          trade: t0 = now, step, t1 = now, store t1 - t0
 * Note:    now_ns() itself costs ~20-50 ns (Windows QPC ticks every 100 ns),
 *          so very short times are rounded to the clock's resolution
 * Returns: 1 on success, 0 on error
 */
static int measure(Run *r, const Feed *feed, int short_win, int long_win,
                   long long *price_out) {
    Engine eng;
    long long i, price;
    uint64_t pass0, t0, t1;

    if (!engine_init(&eng, r->kind, short_win, long_win, feed)) {
        return 0;
    }
    pass0 = now_ns();
    for (i = 0; i < feed->n; i++) {
        t0 = now_ns();
        r->sig[i] = (signed char)engine_step(&eng, feed, i, &price);
        t1 = now_ns();
        r->react_ns[i] = (uint32_t)(t1 - t0);
        if (price_out != NULL) {
            price_out[i] = price;
        }
    }
    r->total_ms = (now_ns() - pass0) / 1e6;

    for (i = 0; i < feed->n; i++) {
        lat_add(&r->stats, r->react_ns[i]);
    }
    r->buys = engine_buys(&eng);
    r->sells = engine_sells(&eng);
    r->vwap = engine_vwap(&eng);
    engine_free(&eng);
    return 1;
}

/* simulate
 * What:    the rival-bot P&L for one version and one R
 * In:      sig[] = the trading signals, react[] = this version's reaction
 *          times, price[] = every trade's price in cents
 * Steps:   for each trade: fill any order lost on the previous signal at
 *          this trade's price, then react to this trade's signal
 */
static Rival simulate(const signed char *sig, const uint32_t *react,
                      const long long *price, long long n, double r_us) {
    Rival rv;
    long long i;

    rival_init(&rv, r_us);
    for (i = 0; i < n; i++) {
        rival_on_trade(&rv, price[i]);
        if (sig[i] != SIG_NONE) {
            rival_on_signal(&rv, sig[i], react[i], price[i]);
        }
    }
    return rv;
}

/* free_runs - release every array (safe on NULLs) */
static void free_runs(Run *runs, int count) {
    int k;
    for (k = 0; k < count; k++) {
        free(runs[k].react_ns);
        free(runs[k].sig);
        lat_free(&runs[k].stats);
    }
}

int main(int argc, char *argv[]) {
    const char *path = NULL;
    const char *out_path = "stageC/rival_sweep.dat";
    int short_win = 35, long_win = 60;
    Feed feed;
    Run runs[N_ENGINES];
    long long *price;
    long long i, v0_diff = 0;
    const signed char *ref_sig;
    FILE *out;
    int k, step, ok = 1;

    for (k = 1; k < argc; k++) {
        if (strcmp(argv[k], "--short") == 0 && k + 1 < argc) {
            short_win = atoi(argv[++k]);
        } else if (strcmp(argv[k], "--long") == 0 && k + 1 < argc) {
            long_win = atoi(argv[++k]);
        } else if (strcmp(argv[k], "--out") == 0 && k + 1 < argc) {
            out_path = argv[++k];
        } else if (argv[k][0] != '-' && path == NULL) {
            path = argv[k];
        } else {
            path = NULL;
            break;
        }
    }
    if (path == NULL || short_win < 1 || short_win >= long_win || long_win > MAX_WIN) {
        printf("usage: %s <file.csv> [--short S] [--long L] [--out file.dat]\n", argv[0]);
        return 1;
    }

    if (!feed_load(&feed, path)) {
        return 1;
    }
    printf("Stage C rival bot  file=%s  trades=%lld  windows=%d/%d\n",
           path, feed.n, short_win, long_win);

    /* ---- 1) measure every version ---- */
    price = malloc((size_t)feed.n * sizeof(long long));
    for (k = 0; k < N_ENGINES; k++) {
        runs[k].kind = (EngineKind)k;          /* ENGINE_V0, ENGINE_FAST, ENGINE_MEM */
        runs[k].react_ns = malloc((size_t)feed.n * sizeof(uint32_t));
        runs[k].sig = malloc((size_t)feed.n);
        if (!lat_init(&runs[k].stats, feed.n) || runs[k].react_ns == NULL ||
            runs[k].sig == NULL || price == NULL) {
            ok = 0;
        }
    }
    if (!ok) {
        printf("malloc failed\n");
        free_runs(runs, N_ENGINES);
        free(price);
        feed_free(&feed);
        return 1;
    }
    for (k = 0; k < N_ENGINES && ok; k++) {
        printf("timing %-4s ... ", engine_name(runs[k].kind));
        fflush(stdout);
        /* prices are taken from the exact fixed-point MEM engine */
        ok = measure(&runs[k], &feed, short_win, long_win,
                     runs[k].kind == ENGINE_MEM ? price : NULL);
        printf("%.1f ms\n", runs[k].total_ms);
    }
    if (!ok) {
        free_runs(runs, N_ENGINES);
        free(price);
        feed_free(&feed);
        return 1;
    }

    /* ---- 2) signals ---- */
    ref_sig = runs[ENGINE_MEM].sig;
    for (i = 0; i < feed.n; i++) {
        if (runs[ENGINE_V0].sig[i] != ref_sig[i]) {
            v0_diff++;
        }
    }

    printf("\n%-5s %10s %10s %10s %10s %10s %10s %16s\n", "ver", "avg ns", "p50 ns",
           "p99 ns", "worst ns", "BUY", "SELL", "final VWAP");
    for (k = 0; k < N_ENGINES; k++) {
        Run *r = &runs[k];
        printf("%-5s %10.1f %10.0f %10.0f %10llu %10d %10d %16.8f\n",
               engine_name(r->kind), lat_mean(&r->stats), lat_percentile(&r->stats, 50),
               lat_percentile(&r->stats, 99), (unsigned long long)r->stats.max,
               r->buys, r->sells, r->vwap);
    }
    printf("\nTrading signals used for every version: exact fixed-point (%d BUY, %d SELL).\n",
           runs[ENGINE_MEM].buys, runs[ENGINE_MEM].sells);
    printf("Version 0's own double/naive engine disagrees on %lld trades "
           "(rounding when short SMA == long SMA).\n", v0_diff);

    /* ---- 3) standard rival, R = 1 us ---- */
    {
        Rival r0 = simulate(ref_sig, runs[ENGINE_V0].react_ns, price, feed.n, STANDARD_R);
        Rival rf = simulate(ref_sig, runs[ENGINE_MEM].react_ns, price, feed.n, STANDARD_R);
        Rival rq = simulate(ref_sig, runs[ENGINE_FAST].react_ns, price, feed.n, STANDARD_R);
        Rival own = simulate(runs[ENGINE_V0].sig, runs[ENGINE_V0].react_ns, price,
                             feed.n, STANDARD_R);

        printf("\nRival R = %.1f us\n", STANDARD_R);
        printf("  %-5s P&L %+12.2f USDT  won %6d  lost %6d  round trips %d\n", "v0",
               r0.pnl / (double)PRICE_SCALE, r0.wins, r0.losses, r0.round_trips);
        printf("  %-5s P&L %+12.2f USDT  won %6d  lost %6d  round trips %d\n", "fast",
               rq.pnl / (double)PRICE_SCALE, rq.wins, rq.losses, rq.round_trips);
        printf("  %-5s P&L %+12.2f USDT  won %6d  lost %6d  round trips %d\n", "mem",
               rf.pnl / (double)PRICE_SCALE, rf.wins, rf.losses, rf.round_trips);
        printf("  money lost to slow code = P&L(mem) - P&L(v0) = %+.2f USDT\n",
               (rf.pnl - r0.pnl) / (double)PRICE_SCALE);
        printf("  (v0 trading on its OWN signals instead: P&L %+.2f USDT, %d round trips)\n",
               own.pnl / (double)PRICE_SCALE, own.round_trips);
    }

    /* ---- 4) sweep R and write the .dat file ---- */
    out = fopen(out_path, "w");
    if (out == NULL) {
        printf("cannot write %s\n", out_path);
    } else {
        fprintf(out, "# R_us  pnl_v0  pnl_fast  pnl_mem  money_lost(mem-v0)  "
                     "win%%_v0  win%%_fast  win%%_mem   (USDT, file %s)\n", path);
    }
    printf("\n%8s %12s %12s %12s %12s %7s %7s %7s\n", "R (us)", "P&L v0", "P&L fast",
           "P&L mem", "lost", "win v0", "fast", "mem");
    for (step = 0; step <= R_DECADES * R_PER_DEC; step++) {
        double r_us = R_MIN_US * pow(10.0, (double)step / R_PER_DEC);
        Rival rv[N_ENGINES];
        double win_pct[N_ENGINES];

        for (k = 0; k < N_ENGINES; k++) {
            rv[k] = simulate(ref_sig, runs[k].react_ns, price, feed.n, r_us);
            win_pct[k] = 100.0 * rv[k].wins / (rv[k].wins + rv[k].losses);
        }
        printf("%8.3f %+12.2f %+12.2f %+12.2f %+12.2f %6.1f%% %6.1f%% %6.1f%%\n", r_us,
               rv[ENGINE_V0].pnl / 100.0, rv[ENGINE_FAST].pnl / 100.0,
               rv[ENGINE_MEM].pnl / 100.0,
               (rv[ENGINE_MEM].pnl - rv[ENGINE_V0].pnl) / 100.0,
               win_pct[ENGINE_V0], win_pct[ENGINE_FAST], win_pct[ENGINE_MEM]);
        if (out != NULL) {
            fprintf(out, "%.4f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", r_us,
                    rv[ENGINE_V0].pnl / 100.0, rv[ENGINE_FAST].pnl / 100.0,
                    rv[ENGINE_MEM].pnl / 100.0,
                    (rv[ENGINE_MEM].pnl - rv[ENGINE_V0].pnl) / 100.0,
                    win_pct[ENGINE_V0], win_pct[ENGINE_FAST], win_pct[ENGINE_MEM]);
        }
    }
    if (out != NULL) {
        fclose(out);
        printf("\nwrote %s (plot with: gnuplot stageC/plot_rival.gnuplot)\n", out_path);
    }

    free_runs(runs, N_ENGINES);
    free(price);
    feed_free(&feed);
    return 0;
}
