/*
 * stageC_malloc.c - Stage C in-memory engine with a malloc'd array (Leroi)
 *
 * Exactly the same as stageB/stageB_inmem.c (double prices, running-sum SMA),
 * except the trades are stored in an array allocated with malloc to fit the
 * file, instead of a fixed 10-million-trade global array. So any difference
 * in memory or time between the two comes only from how the array is made.
 *
 *   1) LOAD:    count the file, malloc the array, fill it
 *   2) PROCESS: loop over the array (no file reading here)
 *   3) FREE:    give the memory back
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageC/stageC_malloc.c stageC/engine/trades_malloc.c
 *       stageB/engine/vwap.c stageB/engine/ohlc.c stageB/engine/sma.c
 *       stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c
 *       -o stageC/stageC_malloc
 * Run (from the project root):
 *   ./stageC/stageC_malloc            test20, dev_1M and stress_full
 *   ./stageC/stageC_malloc dev        test20 and dev_1M only (for valgrind,
 *                                     which is far too slow for stress_full)
 *
 * Expected: same output as stageB_inmem: test20 table matches
 *           data/group_data/test20_expected.txt, dev_1M gives final
 *           VWAP 76217.01291587, 2465 BUY, 2464 SELL.
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* free */
#include <string.h>                      /* strcmp */
#include "engine/trades_malloc.h"
#include "../stageB/engine/vwap.h"
#include "../stageB/engine/ohlc.h"
#include "../stageB/engine/sma.h"
#include "../stageB/engine/signal.h"
#include "../stageB/engine/report.h"

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
 * process - run the engine over list[0] .. list[n-1]
 *
 * The same loop as stageB_inmem.c. The only change is that the array is
 * passed in as a pointer, because it is no longer a global.
 */
static void process(const Trade *list, long long n, int short_win, int long_win,
                    int print_table) {
    Vwap vwap;
    OhlcBar bar, finished;
    Sma short_sma, long_sma;
    Crossover cross;
    int signals_printed = 0;
    long long i;

    vwap_init(&vwap);
    ohlc_init(&bar);
    ohlc_init(&finished);
    sma_init(&short_sma, short_win);
    sma_init(&long_sma, long_win);
    crossover_init(&cross);

    if (print_table) {
        print_table_header();
    }

    for (i = 0; i < n; i++) {
        const Trade *t = &list[i];    /* the current trade */
        long long trade_no = i + 1;   /* trade numbers start at 1 */
        int signal = SIG_NONE;
        double short_value = 0.0, long_value = 0.0;
        int have_short, have_long;

        vwap_update(&vwap, t->price, t->qty);

        if (ohlc_update(&bar, t->time, t->price, &finished) && print_table) {
            print_bar(finished.sec, finished.open, finished.high, finished.low,
                      finished.close, finished.n);
        }

        sma_push(&short_sma, t->price);
        sma_push(&long_sma, t->price);
        have_short = sma_ready(&short_sma);
        have_long  = sma_ready(&long_sma);
        if (have_short) short_value = sma_value(&short_sma);
        if (have_long)  long_value  = sma_value(&long_sma);

        if (have_short && have_long) {
            signal = crossover_update(&cross, short_value, long_value);
        }

        if (print_table) {
            print_trade_row(trade_no, t->price, t->qty, t->is_buy,
                            vwap_value(&vwap), have_short, short_value,
                            have_long, long_value, signal);
        } else if (signal != SIG_NONE && signals_printed < 10) {
            printf("  signal %-4s at trade %lld\n", signal_name(signal), trade_no);
            signals_printed++;
        }
    }

    /* the last bar never gets "finished" by a new second, so print it here */
    if (print_table && bar.active) {
        print_bar_header();
        print_bar(bar.sec, bar.open, bar.high, bar.low, bar.close, bar.n);
    }

    print_summary(vwap_value(&vwap), cross.buys, cross.sells);
}

/*
 * run - load one file into a malloc'd array, process it, then free it
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t t0, t1, t2;
    long long n;
    Trade *list;

    if (short_win >= long_win || long_win > MAX_WIN) {
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    t0 = now_ns();
    list = load_trades_malloc(path, &n);   /* count + malloc + fill */
    t1 = now_ns();
    if (list == NULL) {
        return 1;                          /* the loader already freed everything */
    }

    process(list, n, short_win, long_win, print_table);
    t2 = now_ns();

    printf("Trades loaded: %lld\n", n);
    printf("Array size:   %lld x %d B = %.1f MB (malloc)\n",
           n, (int)sizeof(Trade), (double)n * sizeof(Trade) / 1e6);
    printf("Load time:    %.3f ms (count + malloc + fill)\n", (t1 - t0) / 1e6);
    printf("Process time: %.3f ms\n", (t2 - t1) / 1e6);

    /* give the memory back before the next file is loaded */
    free(list);
    return 0;
}

int main(int argc, char *argv[]) {
    /* "dev" as the first argument skips stress_full (valgrind runs) */
    int skip_stress = (argc > 1 && strcmp(argv[1], "dev") == 0);

    printf("SMA version: running sum, array: malloc sized to file\n\n");

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
