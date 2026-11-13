/*
 * stageC_rival.c - "slow code loses money": rival bot P&L and the R sweep (Lucas)
 *
 * Steps:
 *   1) LOAD:   load_trades_fx() (engine/trades_fx_fread.c) reads the whole
 *              file with one fread into a malloc'd TradeFx array (cents)
 *   2) TIME:   run every trade through each version as fast as possible and
 *              time EACH trade with now_ns(). That per-trade processing time
 *              is the version's reaction time (brief, rival bot rule 2).
 *                v0  = Version 0 (engine/stream_engine.c): next_trade() reads
 *                      each line, double prices, naive SMA (ladder V0)
 *                mem = fastest version (engine/fixed_engine.c): the TradeFx
 *                      array + the fixed-point modules (ladder V7)
 *   3) SIGNALS: the brief says both versions raise exactly the same signals.
 *              Ours do not quite: Version 0 uses double + naive SMA, and when
 *              short SMA == long SMA, rounding gives d a random tiny sign (see
 *              docs/leroi_stageB_C.md, section 2). So the trading signals for
 *              BOTH versions are mem's exact fixed-point ones, and each version
 *              only brings its own reaction times. The P&L difference then
 *              comes only from speed, as the brief intends. The program also
 *              prints how many trades Version 0's own signals got different.
 *   4) R = 1 us: P&L of each version and money lost = P&L(mem) - P&L(v0)
 *   5) SWEEP:  R from 0.1 us to 100 us (10 steps per power of ten), written
 *              to stageC/rival_sweep.dat for gnuplot (stageC/plot_rival.gnuplot)
 *
 * Usage (from the project root):
 *   stageC_rival <file.csv> [--short S] [--long L] [--out file.dat]
 * Example:
 *   ./stageC/stageC_rival data/stress_full.csv
 *   gnuplot stageC/plot_rival.gnuplot        -> stageC/rival_sweep.png
 * On the Pi: taskset -c 2 ./stageC/stageC_rival data/stress_full.csv
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* malloc, free, atoi */
#include <string.h>                      /* strcmp */
#include <math.h>                        /* pow */
#include "engine/signal.h"               /* SIG_BUY, SIG_SELL, SIG_NONE */
#include "engine/sma.h"                  /* MAX_WIN */
#include "engine/fixed.h"                /* PRICE_SCALE (cents) */
#include "engine/trades_fx.h"            /* TradeFx, load_trades_fx() (fread loader) */
#include "engine/stream_engine.h"        /* Version 0 (ladder V0) */
#include "engine/fixed_engine.h"         /* fastest version (ladder V7) */
#include "engine/rival.h"                /* the rival bot */

#define R_MIN_US    0.1          /* smallest rival reaction time in the sweep */
#define R_STEPS     30           /* 0.1 us -> 100 us = 3 powers of ten x 10 steps */
#define R_PER_TEN   10           /* steps per power of ten */
#define STANDARD_R  1.0          /* brief: the standard rival is 1 us */

/* simulate
 * What:    the rival bot P&L for one version and one rival time R
 * In:      sig[i]   = the trading signal on trade i
 *          react[i] = this version's reaction time on trade i, ns
 *          price[i] = trade i's price in cents
 * Steps:   for each trade: fill any order we lost on the previous signal at
 *          this trade's price, then react to this trade's signal
 */
Rival simulate(int sig[], long long react[], long long price[], long long n, double r_us) {
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

/* average and worst (largest) of n times */
double average(long long times[], long long n) {
    double total = 0.0;
    long long i;

    for (i = 0; i < n; i++) {
        total = total + times[i];
    }
    return total / n;
}

long long worst(long long times[], long long n) {
    long long most = 0;
    long long i;

    for (i = 0; i < n; i++) {
        if (times[i] > most) {
            most = times[i];
        }
    }
    return most;
}

/* win_percent - how many of the orders beat the rival, in % */
double win_percent(Rival *rv) {
    int orders = rv->wins + rv->losses;

    if (orders == 0) {
        return 0.0;
    }
    return 100.0 * rv->wins / orders;
}

/* print_pnl - one line of the R = 1 us table */
void print_pnl(char *name, Rival *rv) {
    printf("  %-5s P&L %+12.2f USDT  won %6d  lost %6d  round trips %d\n", name,
           (double)rv->pnl / PRICE_SCALE, rv->wins, rv->losses, rv->round_trips);
}

int main(int argc, char *argv[]) {
    char *path = NULL;
    char *out_path = "stageC/rival_sweep.dat";
    int short_win = 35, long_win = 60;
    TradeFx *list;                     /* malloc'd array from load_trades_fx() */
    long long n, i, v0_diff = 0;
    long long t0, t1, pass_start;
    double v0_ms, mem_ms;
    long long *v0_react, *mem_react;     /* reaction time of every trade  */
    int *v0_sig, *mem_sig;               /* signal of every trade         */
    long long *price;                    /* price of every trade, cents   */
    int v0_buy, v0_sell, mem_buy, mem_sell;
    double v0_vw, mem_vw, r_us;
    Rival r_v0, r_mem, r_own;
    FILE *out;
    int k, step;

    /* ---- read the command line ---- */
    k = 1;
    while (k < argc) {
        if (strcmp(argv[k], "--short") == 0 && k + 1 < argc) {
            short_win = atoi(argv[k + 1]);
            k++;
        } else if (strcmp(argv[k], "--long") == 0 && k + 1 < argc) {
            long_win = atoi(argv[k + 1]);
            k++;
        } else if (strcmp(argv[k], "--out") == 0 && k + 1 < argc) {
            out_path = argv[k + 1];
            k++;
        } else if (argv[k][0] != '-' && path == NULL) {
            path = argv[k];
        } else {
            path = NULL;                 /* unknown argument: show the usage */
            k = argc;
        }
        k++;
    }
    if (path == NULL || short_win < 1 || short_win >= long_win || long_win > MAX_WIN) {
        printf("usage: %s <file.csv> [--short S] [--long L] [--out file.dat]\n", argv[0]);
        return 1;
    }

    /* ---- 1) LOAD ---- */
    list = load_trades_fx(path, &n);
    if (list == NULL) {
        return 1;
    }
    printf("Stage C rival bot  file=%s  trades=%lld  windows=%d/%d\n",
           path, n, short_win, long_win);

    /* one value per trade for each array */
    v0_react = malloc(n * sizeof(long long));
    mem_react = malloc(n * sizeof(long long));
    v0_sig = malloc(n * sizeof(int));
    mem_sig = malloc(n * sizeof(int));
    price = malloc(n * sizeof(long long));
    if (v0_react == NULL || mem_react == NULL || v0_sig == NULL || mem_sig == NULL ||
        price == NULL) {
        printf("malloc failed\n");
        free(v0_react);
        free(mem_react);
        free(v0_sig);
        free(mem_sig);
        free(price);
        free_trades_fx(list);
        return 1;
    }

    /* ---- 2a) time Version 0 on every trade ---- */
    if (!stream_start(path, short_win, long_win)) {
        return 1;
    }
    pass_start = now_ns();
    for (i = 0; i < n; i++) {
        t0 = now_ns();
        v0_sig[i] = stream_step();
        t1 = now_ns();
        v0_react[i] = t1 - t0;
    }
    v0_ms = (now_ns() - pass_start) / 1e6;
    stream_stop();
    v0_buy = stream_buys();
    v0_sell = stream_sells();
    v0_vw = stream_vwap();
    printf("timing v0   ... %.1f ms\n", v0_ms);

    /* ---- 2b) time the fastest version on every trade ---- */
    fixed_start(short_win, long_win);
    pass_start = now_ns();
    for (i = 0; i < n; i++) {
        t0 = now_ns();
        mem_sig[i] = fixed_step(&list[i]);
        t1 = now_ns();
        mem_react[i] = t1 - t0;
    }
    mem_ms = (now_ns() - pass_start) / 1e6;
    mem_buy = fixed_buys();
    mem_sell = fixed_sells();
    mem_vw = fixed_vwap();
    printf("timing mem  ... %.1f ms\n", mem_ms);

    /* prices for the P&L (not timed) */
    for (i = 0; i < n; i++) {
        price[i] = fixed_price_cents(&list[i]);
    }

    /* ---- 3) signals ---- */
    for (i = 0; i < n; i++) {
        if (v0_sig[i] != mem_sig[i]) {
            v0_diff++;
        }
    }
    printf("\n%-5s %10s %10s %10s %10s %16s\n", "ver", "avg ns", "worst ns", "BUY", "SELL",
           "final VWAP");
    printf("%-5s %10.1f %10lld %10d %10d %16.8f\n", "v0", average(v0_react, n),
           worst(v0_react, n), v0_buy, v0_sell, v0_vw);
    printf("%-5s %10.1f %10lld %10d %10d %16.8f\n", "mem", average(mem_react, n),
           worst(mem_react, n), mem_buy, mem_sell, mem_vw);
    printf("\nTrading signals used for both versions: exact fixed-point (%d BUY, %d SELL).\n",
           mem_buy, mem_sell);
    printf("Version 0's own double/naive engine disagrees on %lld trades "
           "(rounding when short SMA == long SMA).\n", v0_diff);

    /* ---- 4) the standard rival, R = 1 us ---- */
    r_v0 = simulate(mem_sig, v0_react, price, n, STANDARD_R);
    r_mem = simulate(mem_sig, mem_react, price, n, STANDARD_R);
    r_own = simulate(v0_sig, v0_react, price, n, STANDARD_R);
    printf("\nRival R = %.1f us\n", STANDARD_R);
    print_pnl("v0", &r_v0);
    print_pnl("mem", &r_mem);
    printf("  money lost to slow code = P&L(mem) - P&L(v0) = %+.2f USDT\n",
           (double)(r_mem.pnl - r_v0.pnl) / PRICE_SCALE);
    printf("  (v0 trading on its OWN signals instead: P&L %+.2f USDT, %d round trips)\n",
           (double)r_own.pnl / PRICE_SCALE, r_own.round_trips);

    /* ---- 5) sweep R and write the .dat file for gnuplot ---- */
    out = fopen(out_path, "w");
    if (out == NULL) {
        printf("cannot write %s\n", out_path);
    } else {
        fprintf(out, "# R_us  pnl_v0  pnl_mem  money_lost(mem-v0)  win%%_v0  win%%_mem"
                     "   (USDT, file %s)\n", path);
    }
    printf("\n%8s %12s %12s %12s %7s %7s\n", "R (us)", "P&L v0", "P&L mem", "lost",
           "win v0", "mem");
    for (step = 0; step <= R_STEPS; step++) {
        r_us = R_MIN_US * pow(10.0, (double)step / R_PER_TEN);
        r_v0 = simulate(mem_sig, v0_react, price, n, r_us);
        r_mem = simulate(mem_sig, mem_react, price, n, r_us);
        printf("%8.3f %+12.2f %+12.2f %+12.2f %6.1f%% %6.1f%%\n", r_us,
               (double)r_v0.pnl / PRICE_SCALE, (double)r_mem.pnl / PRICE_SCALE,
               (double)(r_mem.pnl - r_v0.pnl) / PRICE_SCALE,
               win_percent(&r_v0), win_percent(&r_mem));
        if (out != NULL) {
            fprintf(out, "%.4f %.2f %.2f %.2f %.2f %.2f\n", r_us,
                    (double)r_v0.pnl / PRICE_SCALE, (double)r_mem.pnl / PRICE_SCALE,
                    (double)(r_mem.pnl - r_v0.pnl) / PRICE_SCALE,
                    win_percent(&r_v0), win_percent(&r_mem));
        }
    }
    if (out != NULL) {
        fclose(out);
        printf("\nwrote %s (plot with: gnuplot stageC/plot_rival.gnuplot)\n", out_path);
    }

    free(v0_react);
    free(mem_react);
    free(v0_sig);
    free(mem_sig);
    free(price);
    free_trades_fx(list);
    return 0;
}
