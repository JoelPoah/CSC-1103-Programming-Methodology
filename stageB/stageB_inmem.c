/*
 * stageB_inmem.c - Stage B in-memory engine (Leroi)
 *
 * Same calculations as stageB_manual.c, but in two separate phases:
 *   1) LOAD:    read the whole CSV into the global trades[] array
 *   2) PROCESS: loop over the array (no file reading here)
 * Each phase is timed with now_ns(), so we can see how much of the old
 * run time was file reading and how much was the actual maths.
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageB/stageB_inmem.c stageB/engine/trades.c stageB/engine/vwap.c
 *       stageB/engine/ohlc.c stageB/engine/sma.c stageB/engine/signal.c
 *       stageB/engine/report.c helper_function/trade_io.c -o stageB/stageB_inmem
 * Run (from the project root):
 *   ./stageB/stageB_inmem
 *
 * Naive SMA version (for the SMA comparison): same build, plus
 *   -DNAIVE_SMA stageB/engine/sma_naive.c  and  -o stageB/stageB_inmem_naive
 * -DNAIVE_SMA tells the compiler to use the #ifdef NAIVE_SMA code below,
 * so both versions come from one source file and differ ONLY in the SMA.
 *
 * Expected: test20 table matches data/group_data/test20_expected.txt,
 *           dev_1M gives final VWAP 76217.01291587, 2465 BUY, 2464 SELL.
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include "engine/trades.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#ifdef NAIVE_SMA
#include "engine/sma_naive.h"   /* only needed by the naive build */
#endif
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
 * process - run the engine over trades[0] .. trades[n-1]
 *
 * This is the main loop from stageB_manual.c, except each trade now comes
 * from the array instead of from next_trade().
 */
static void process(long long n, int short_win, int long_win, int print_table) {
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
        /* a pointer to the current trade, so we write t->price
         * instead of trades[i].price every time */
        Trade *t = &trades[i];
        long long trade_no = i + 1;   /* trade numbers start at 1 */
        int signal = SIG_NONE;
        double short_value = 0.0, long_value = 0.0;
        int have_short, have_long;

        vwap_update(&vwap, t->price, t->qty);

        if (ohlc_update(&bar, t->time, t->price, &finished) && print_table) {
            print_bar(finished.sec, finished.open, finished.high, finished.low,
                      finished.close, finished.n);
        }

#ifdef NAIVE_SMA
        /* slow version: re-add the whole window every trade */
        sma_naive_push(&short_sma, t->price);
        sma_naive_push(&long_sma, t->price);
        have_short = sma_ready(&short_sma);
        have_long  = sma_ready(&long_sma);
        if (have_short) short_value = sma_naive_value(&short_sma);
        if (have_long)  long_value  = sma_naive_value(&long_sma);
#else
        /* fast version: running sum, one add + one subtract per trade */
        sma_push(&short_sma, t->price);
        sma_push(&long_sma, t->price);
        have_short = sma_ready(&short_sma);
        have_long  = sma_ready(&long_sma);
        if (have_short) short_value = sma_value(&short_sma);
        if (have_long)  long_value  = sma_value(&long_sma);
#endif

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
 * run - load one file, then process it, timing both phases
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t t0, t1, t2;
    long long n;

    if (short_win >= long_win || long_win > MAX_WIN) {
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    t0 = now_ns();
    n = load_trades(path);
    t1 = now_ns();
    if (n < 0) {
        return 1;
    }

    process(n, short_win, long_win, print_table);
    t2 = now_ns();

    printf("Trades loaded: %lld\n", n);
    printf("Load time:    %.3f ms\n", (t1 - t0) / 1e6);
    printf("Process time: %.3f ms\n", (t2 - t1) / 1e6);
    return 0;
}

int main(void) {
#ifdef NAIVE_SMA
    printf("SMA version: naive (re-add whole window)\n\n");
#else
    printf("SMA version: running sum\n\n");
#endif
    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1) != 0) return 1;

    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;

    printf("\n=== stress_full.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(STRESS_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;

    return 0;
}
