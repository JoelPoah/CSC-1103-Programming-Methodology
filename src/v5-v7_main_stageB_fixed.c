/*
 * v5-v7_main_stageB_fixed.c - V5, V6, V7: in-memory engine with FIXED-POINT prices (Leroi)
 *
 * From stageB/stageB_fixed.c. Same steps as V4 (load the file into an array,
 * then process the array with a running-sum SMA), but every price and
 * quantity is a long long whole number instead of a double:
 *   price -> cents (x 100), qty -> x 100000   (see engine_fx/fixed.h)
 * Doubles are only used to PRINT results; all the maths in the loop is
 * whole-number maths. Trades are parsed straight to fixed-point
 * (reader/v5-v7_read_trade_fx.c, Lucas), so no double is made while loading either.
 *
 * One source file, several versions, picked at link time (same store/trades_fx.h):
 *   V5: link store/v5_trades_fx_array.c   (fixed-size global array)
 *   V6: link store/v6_trades_fx_malloc.c  (malloc'd array sized to the file, Stage C)
 *   V7: link store/v7_trades_fx_fread.c   (same array, whole file read with
 *       one fread, Lucas's Stage C feed)
 *   (V8 would be the engine_fx kernels in AArch64 assembly, engine_asm/, not written yet)
 *
 * Build (from the project root):
 *   make ladder
 *   or: gcc -O2 -Wall src/v5-v7_main_stageB_fixed.c src/store/v5_trades_fx_array.c
 *       src/reader/v5-v7_read_trade_fx.c src/engine_fx/vwap_fx.c src/engine_fx/ohlc_fx.c
 *       src/engine_fx/sma_fx.c src/engine_fx/signal_fx.c src/engine/report.c
 *       -o bin/v5_stageB_fixed_point
 *   (V6: v6_trades_fx_malloc.c instead of v5_trades_fx_array.c, -o bin/v6_stageC_malloc)
 *   (V7: v7_trades_fx_fread.c instead of v5_trades_fx_array.c, -o bin/v7_stageC_fread)
 * Run (from the project root):
 *   ./bin/v5_stageB_fixed_point        test20, dev_1M and stress_full
 *   ./bin/v6_stageC_malloc dev         test20 and dev_1M only (for valgrind,
 *                                      which is far too slow for stress_full)
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>                      /* strcmp */
#include "config.h"                      /* file paths and SMA windows */
#include "engine_fx/fixed.h"
#include "store/trades_fx.h"
#include "engine_fx/vwap_fx.h"
#include "engine_fx/ohlc_fx.h"
#include "engine_fx/sma_fx.h"
#include "engine_fx/signal_fx.h"
#include "engine/report.h"

/* print_bar_fx - print a fixed-point bar using the shared report function */
static void print_bar_fx(const OhlcBarFx *b) {
    print_bar(b->sec, (double)b->open / PRICE_SCALE, (double)b->high / PRICE_SCALE,
              (double)b->low / PRICE_SCALE, (double)b->close / PRICE_SCALE, b->n);
}

/*
 * process - run the engine over trades[0] .. trades[n-1]
 */
static void process(const TradeFx *trades, long long n, int short_win, int long_win,
                    int print_table) {
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
        const TradeFx *t = &trades[i];
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
            print_signal(signal, trade_no);
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
    TradeFx *trades;

    if (short_win >= long_win || long_win > MAX_WIN) {
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    t0 = now_ns();
    trades = load_trades_fx(path, &n);
    t1 = now_ns();
    if (trades == NULL) {
        return 1;
    }

    process(trades, n, short_win, long_win, print_table);
    t2 = now_ns();
    free_trades_fx(trades);

    printf("Trades loaded: %lld\n", n);
    printf("Load time:    %.3f ms\n", (t1 - t0) / 1e6);
    printf("Process time: %.3f ms\n", (t2 - t1) / 1e6);
    return 0;
}

int main(int argc, char *argv[]) {
    /* "dev" as the first argument skips stress_full (valgrind runs) */
    int skip_stress = (argc > 1 && strcmp(argv[1], "dev") == 0);

    printf("Prices: fixed-point (price x %lld, qty x %lld)\n\n", PRICE_SCALE, QTY_SCALE);

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
