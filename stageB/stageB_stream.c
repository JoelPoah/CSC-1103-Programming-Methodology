/*
 * stageB_stream.c - streaming engine, for timing against stageB_inmem (Leroi)
 *
 * The same calculations as stageB_inmem.c (double prices, running-sum SMA,
 * same engine modules), but each trade is read from the file with
 * next_trade() and processed straight away, without storing it. This is the
 * "re-reading the file" side of the in-memory comparison.
 *
 * Why not just time stageB_manual.c? It writes signals.dat and runs gnuplot,
 * and the brief says never to write plot files in a version you time.
 * Here the only difference from stageB_inmem is "read while processing"
 * vs "load first, then process", so the comparison is fair.
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageB/stageB_stream.c stageB/engine/vwap.c stageB/engine/ohlc.c
 *       stageB/engine/sma.c stageB/engine/signal.c stageB/engine/report.c
 *       helper_function/trade_io.c -o stageB/stageB_stream
 * Run (from the project root):
 *   ./stageB/stageB_stream            test20, dev_1M and stress_full
 *   ./stageB/stageB_stream dev        test20 and dev_1M only
 *
 * Expected: same output as stageB_inmem (test20 matches test20_expected.txt,
 *           dev_1M gives VWAP 76217.01291587, 2465 BUY, 2464 SELL).
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>                      /* strcmp */
#include "../helper_function/trade_io.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/* ---- file paths (relative to the project root) ---- */
#define TEST20_PATH "data/group_data/test20.csv"
#define DEV_PATH    "data/group_data/dev_1M.csv"
#define STRESS_PATH "data/group_data/stress_full.csv"

/* ---- SMA windows (Group 06) ---- */
#define TEST_SHORT 3
#define TEST_LONG  5
#define DEV_SHORT  35
#define DEV_LONG   60

/*
 * process_file - read the open file one trade at a time and run the engine
 * Returns how many trades were processed.
 *
 * The loop body is the same as process() in stageB_inmem.c, except the
 * values come from trade_price() etc. instead of from trades[i].
 */
static long long process_file(int short_win, int long_win, int print_table) {
    Vwap vwap;
    OhlcBar bar, finished;
    Sma short_sma, long_sma;
    Crossover cross;
    int signals_printed = 0;
    long long n = 0;

    vwap_init(&vwap);
    ohlc_init(&bar);
    ohlc_init(&finished);
    sma_init(&short_sma, short_win);
    sma_init(&long_sma, long_win);
    crossover_init(&cross);

    if (print_table) {
        print_table_header();
    }

    while (next_trade()) {
        /* copy the current trade into local variables, once */
        long long time   = trade_time();
        double    price  = trade_price();
        double    qty    = trade_qty();
        int       is_buy = trade_is_buy();
        long long trade_no = n + 1;   /* trade numbers start at 1 */
        int signal = SIG_NONE;
        double short_value = 0.0, long_value = 0.0;
        int have_short, have_long;

        vwap_update(&vwap, price, qty);

        if (ohlc_update(&bar, time, price, &finished) && print_table) {
            print_bar(finished.sec, finished.open, finished.high, finished.low,
                      finished.close, finished.n);
        }

        sma_push(&short_sma, price);
        sma_push(&long_sma, price);
        have_short = sma_ready(&short_sma);
        have_long  = sma_ready(&long_sma);
        if (have_short) short_value = sma_value(&short_sma);
        if (have_long)  long_value  = sma_value(&long_sma);

        if (have_short && have_long) {
            signal = crossover_update(&cross, short_value, long_value);
        }

        if (print_table) {
            print_trade_row(trade_no, price, qty, is_buy,
                            vwap_value(&vwap), have_short, short_value,
                            have_long, long_value, signal);
        } else if (signal != SIG_NONE && signals_printed < 10) {
            printf("  signal %-4s at trade %lld\n", signal_name(signal), trade_no);
            signals_printed++;
        }
        n++;
    }

    /* the last bar never gets "finished" by a new second, so print it here */
    if (print_table && bar.active) {
        print_bar_header();
        print_bar(bar.sec, bar.open, bar.high, bar.low, bar.close, bar.n);
    }

    print_summary(vwap_value(&vwap), cross.buys, cross.sells);
    return n;
}

/*
 * run - open one file, stream it through the engine, and time the whole thing
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t t0, t1;
    long long n;

    if (short_win >= long_win || long_win > MAX_WIN) {
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    t0 = now_ns();
    if (!open_trades(path)) {
        return 1;
    }
    n = process_file(short_win, long_win, print_table);
    close_trades();
    t1 = now_ns();

    /* reading and processing are mixed together, so there is only one time */
    printf("Trades processed: %lld\n", n);
    printf("Stream time:  %.3f ms (read + process)\n", (t1 - t0) / 1e6);
    return 0;
}

int main(int argc, char *argv[]) {
    /* "dev" as the first argument skips stress_full (valgrind runs) */
    int skip_stress = (argc > 1 && strcmp(argv[1], "dev") == 0);

    printf("SMA version: running sum, streaming (no array)\n\n");

    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1) != 0) return 1;

    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;

    if (!skip_stress) {
        printf("\n=== stress_full.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
        if (run(STRESS_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;
    }

    return 0;
}
