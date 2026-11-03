/*
 * stageB_fixed.c - Stage B in-memory engine with FIXED-POINT prices (Leroi)
 *
 * Same steps as stageB_inmem.c (load the file into an array, then process
 * the array with a running-sum SMA), but every price and quantity is a
 * long long whole number instead of a double:
 *   price -> cents (x 100), qty -> x 100000   (see engine/fixed.h)
 * Doubles are only used to PRINT results; all the maths in the loop is
 * whole-number maths. This is the comparison against stageB_inmem.c.
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageB/stageB_fixed.c stageB/engine/trades_fx.c
 *       stageB/engine/vwap_fx.c stageB/engine/ohlc_fx.c stageB/engine/sma_fx.c
 *       stageB/engine/signal_fx.c stageB/engine/signal.c stageB/engine/report.c
 *       helper_function/trade_io.c -o stageB/stageB_fixed
 * Run (from the project root):
 *   ./stageB/stageB_fixed
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include "engine/fixed.h"
#include "engine/trades_fx.h"
#include "engine/vwap_fx.h"
#include "engine/ohlc_fx.h"
#include "engine/sma_fx.h"
#include "engine/signal_fx.h"
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

/* print_bar_fx - print a fixed-point bar using the shared report function */
static void print_bar_fx(const OhlcBarFx *b) {
    print_bar(b->sec, (double)b->open / PRICE_SCALE, (double)b->high / PRICE_SCALE,
              (double)b->low / PRICE_SCALE, (double)b->close / PRICE_SCALE, b->n);
}

/*
 * process - run the engine over trades_fx[0] .. trades_fx[n-1]
 */
static void process(long long n, int short_win, int long_win, int print_table) {
    VwapFx vwap;
    OhlcBarFx bar, finished;
    SmaFx short_sma, long_sma;
    CrossoverFx cross;
    int signals_printed = 0;
    long long i;

    vwap_fx_init(&vwap);
    ohlc_fx_init(&bar);
    ohlc_fx_init(&finished);
    sma_fx_init(&short_sma, short_win);
    sma_fx_init(&long_sma, long_win);
    crossover_fx_init(&cross);

    if (print_table) {
        print_table_header();
    }

    for (i = 0; i < n; i++) {
        TradeFx *t = &trades_fx[i];
        long long trade_no = i + 1;
        int signal = SIG_NONE;
        int have_short, have_long;

        vwap_fx_update(&vwap, t->price, t->qty);

        if (ohlc_fx_update(&bar, t->time, t->price, &finished) && print_table) {
            print_bar_fx(&finished);
        }

        sma_fx_push(&short_sma, t->price);
        sma_fx_push(&long_sma, t->price);
        have_short = sma_fx_ready(&short_sma);
        have_long  = sma_fx_ready(&long_sma);

        /* compare the exact sums - no division, no rounding */
        if (have_short && have_long) {
            signal = crossover_fx_update(&cross, sma_fx_sum(&short_sma), short_win,
                                         sma_fx_sum(&long_sma), long_win);
        }

        /* doubles appear only here, when printing the test20 table */
        if (print_table) {
            print_trade_row(trade_no, (double)t->price / PRICE_SCALE,
                            (double)t->qty / QTY_SCALE, t->is_buy,
                            vwap_fx_value(&vwap), have_short, sma_fx_value(&short_sma),
                            have_long, sma_fx_value(&long_sma), signal);
        } else if (signal != SIG_NONE && signals_printed < 10) {
            printf("  signal %-4s at trade %lld\n", signal_name(signal), trade_no);
            signals_printed++;
        }
    }

    if (print_table && bar.active) {
        print_bar_header();
        print_bar_fx(&bar);
    }

    print_summary(vwap_fx_value(&vwap), cross.buys, cross.sells);
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
    n = load_trades_fx(path);
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
    printf("Prices: fixed-point (price x %lld, qty x %lld)\n\n", PRICE_SCALE, QTY_SCALE);

    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1) != 0) return 1;

    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;

    printf("\n=== stress_full.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(STRESS_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;

    return 0;
}
