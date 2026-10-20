/*
 * stageA_stream.c - Stage A: baseline streaming engine
 *
 * One pass over a trade file, one trade at a time, computing:
 *   1) running VWAP            (two running sums)
 *   2) one-second OHLC bars    (four variables, reset every new second)
 *   3) short and long SMA      (circular arrays of the most recent prices)
 *   4) BUY / SELL signals      (short SMA crossing the long SMA), counted
 *   5) signals.dat + signals.png for a ~2000-trade slice (via gnuplot)
 *   6) test20.csv table to compare with data/group_data/test20_expected.txt
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageA/stageA_stream.c helper_function/trade_io.c -o stageA/stageA_stream
 * Run (from the project root, paths below are relative to it):
 *   ./stageA/stageA_stream
 */
#include <stdio.h>
#include <stdlib.h>
#include "../helper_function/trade_io.h"

/* ---- hardcoded paths (relative to the project root) ---- */
#define TEST20_PATH   "data/group_data/test20.csv" //hardcoded path of test 20 
#define DEV_PATH      "data/group_data/dev_1M.csv"
#define SIGNALS_DAT   "stageA/signals.dat"
#define PLOT_SCRIPT   "stageA/plot_signals.gnuplot"

/* ---- SMA windows ---- */
#define TEST_SHORT 3          /* test20.csv */
#define TEST_LONG  5
#define DEV_SHORT  35         /* Group 06: dev_1M.csv and stress_full.csv */
#define DEV_LONG   60

#define MAX_WIN 128           /* size of the circular arrays */

/* ---- slice of dev_1M.csv written to signals.dat ---- */
#define SLICE_START 1         /* first trade number in the slice */
#define SLICE_LEN   2000

static void print_bar(long long sec, double o, double h, double l, double c, int n) {
    printf("%-12lld %-16.8f %-16.8f %-16.8f %-16.8f %d\n", sec, o, h, l, c, n);
}

/*
 * Stream one file.
 *   table   : 1 = print the per-trade table (test20 style), 0 = summary only
 *   slice_* : write trades [slice_from, slice_to] to datpath (datpath NULL = skip)
 * Returns 0 on success, 1 if the file could not be opened.
 */
static int run(const char *path, int S, int L, int table,
               long long slice_from, long long slice_to, const char *datpath) {
    /* 1) VWAP: two running sums */
    double sum_pq = 0.0, sum_q = 0.0;

    /* 2) OHLC: four variables + trade count for the current second */
    long long cur_sec = -1;
    double o = 0, h = 0, lo = 0, c = 0;
    int bar_n = 0;

    /* 3) SMA: circular windows with running sums */
    double win_s[MAX_WIN], win_l[MAX_WIN];
    double sum_s = 0.0, sum_l = 0.0;

    /* 4) signals */
    int have_prev = 0;
    double prev_s = 0.0, prev_l = 0.0;
    int buys = 0, sells = 0;
    int shown = 0;                       /* first few signals printed in summary mode */

    FILE *dat = NULL;
    double vwap = 0.0;

    if (S >= L || L > MAX_WIN) {
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }
    if (!open_trades(path)) return 1;

    if (datpath != NULL) {
        dat = fopen(datpath, "w");
        if (dat != NULL) fprintf(dat, "# trade price sma_short sma_long signal(+1 BUY, -1 SELL, 0 none)\n");
    }

    if (table) {
        printf("%-6s %-16s %-12s %-6s %-16s %-16s %-16s %s\n",
               "trade", "price", "qty", "is_buy", "VWAP",
               "SMA_short", "SMA_long", "signal");
    }

    while (next_trade()) {
        long long i = trade_number();
        double p = trade_price();
        double q = trade_qty();
        long long sec = trade_time() / 1000000LL;
        int sig = 0;
        int have_s, have_l;
        double sma_s = 0.0, sma_l = 0.0;

        /* 1) VWAP */
        sum_pq += p * q;
        sum_q  += q;
        vwap = sum_pq / sum_q;

        /* 2) OHLC: new second -> emit old bar and reset */
        if (sec != cur_sec) {
            if (cur_sec != -1 && table) print_bar(cur_sec, o, h, lo, c, bar_n);
            cur_sec = sec;
            o = h = lo = c = p;
            bar_n = 1;
        } else {
            if (p > h)  h = p;
            if (p < lo) lo = p;
            c = p;
            bar_n++;
        }

        /* 3) SMA: overwrite the oldest slot, keep the sums up to date */
        {
            int is = (int)((i - 1) % S);
            int il = (int)((i - 1) % L);
            if (i > S) sum_s -= win_s[is];
            if (i > L) sum_l -= win_l[il];
            win_s[is] = p;  sum_s += p;
            win_l[il] = p;  sum_l += p;
        }
        have_s = (i >= S);
        have_l = (i >= L);
        if (have_s) sma_s = sum_s / S;
        if (have_l) sma_l = sum_l / L;

        /* 4) crossover signal: needs both SMAs on two trades in a row */
        if (have_s && have_l) {
            if (have_prev) {
                if (prev_s <= prev_l && sma_s > sma_l)      { sig = 1;  buys++;  }
                else if (prev_s >= prev_l && sma_s < sma_l) { sig = -1; sells++; }
            }
            prev_s = sma_s;
            prev_l = sma_l;
            have_prev = 1;
        }

        if (table) {
            printf("%-6lld %-16.8f %-12.8f %-6d %-16.8f ", i, p, q, trade_is_buy(), vwap);
            if (have_s) printf("%-16.8f ", sma_s); else printf("%-16s ", "-");
            if (have_l) printf("%-16.8f ", sma_l); else printf("%-16s ", "-");
            printf("%s\n", sig == 1 ? "BUY" : sig == -1 ? "SELL" : "");
        } else if (sig != 0 && shown < 10) {
            printf("  signal %s at trade %lld\n", sig == 1 ? "BUY " : "SELL", i);
            shown++;
        }

        /* 5) slice for the plot (blank value when an SMA does not exist yet) */
        if (dat != NULL && i >= slice_from && i <= slice_to && have_l) {
            fprintf(dat, "%lld %.8f %.8f %.8f %d\n", i, p, sma_s, sma_l, sig);
        }
    }
    close_trades();

    /* flush the last OHLC bar */
    if (cur_sec != -1 && table) {
        printf("\nOne-second OHLC bars (second = time / 1,000,000)\n");
        printf("%-12s %-16s %-16s %-16s %-16s %s\n", "second", "open", "high", "low", "close", "trades");
        print_bar(cur_sec, o, h, lo, c, bar_n);
    }
    if (dat != NULL) fclose(dat);

    printf("\nFinal VWAP: %.8f\n", vwap);
    printf("Signals: %d BUY, %d SELL\n", buys, sells);
    return 0;
}

int main(void) {
    /* 6) correctness check on the 20-trade file */
    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1, 0, 0, NULL)) return 1;

    /* full dev file + signals.dat slice */
    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0, SLICE_START, SLICE_START + SLICE_LEN - 1, SIGNALS_DAT)) return 1;

    /* 5) draw signals.png with gnuplot */
    printf("\nRunning gnuplot on %s ...\n", PLOT_SCRIPT);
    if (system("gnuplot " PLOT_SCRIPT) != 0) {
        printf("gnuplot failed or is not installed; signals.dat is still written.\n");
    }
    return 0;
}
