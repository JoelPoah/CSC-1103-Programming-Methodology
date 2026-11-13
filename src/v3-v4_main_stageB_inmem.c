/*
 * v3-v4_main_stageB_inmem.c - V3, V4: Stage B in-memory engine, double prices (Leroi)
 *
 * From stageB/stageB_inmem.c. Same calculations as V2, but in two separate phases:
 *   1) LOAD:    read the whole CSV into the global trades[] array
 *               (store/v3-v4_trades.c, using the V2 reader v2-v4_read_trade_fast.c)
 *   2) PROCESS: loop over the array (no file reading here)
 * Each phase is timed with now_ns(), so we can see how much of the old
 * run time was file reading and how much was the actual maths.
 *
 * One source file, two versions, picked at link time (same engine/sma.h):
 *   V3: link engine/v0-v3_sma_naive.c    (re-add the whole window every trade)
 *   V4: link engine/v4_sma_running.c  (running sum, one add + one subtract)
 * so both versions differ ONLY in the SMA.
 *
 * Build (from the project root):
 *   make ladder
 *   or: gcc -O2 -Wall src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c
 *       src/engine/vwap.c src/engine/ohlc.c src/engine/v4_sma_running.c src/engine/signal.c
 *       src/engine/report.c -o bin/v4_stageB_running_sma
 *   (V3: v0-v3_sma_naive.c instead of v4_sma_running.c, -o bin/v3_stageB_in_memory)
 * Run (from the project root):
 *   ./bin/v4_stageB_running_sma        test20, dev_1M and stress_full
 *   ./bin/v4_stageB_running_sma dev    test20 and dev_1M only (for valgrind /
 *                                      memory comparison with V6)
 *
 * Expected: test20 table matches data/group_data/test20_expected.txt,
 *           dev_1M gives final VWAP 76217.01291587 and
 *           V3 (naive):   2509 BUY, 2469 SELL
 *           V4 (running): 2465 BUY, 2464 SELL
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>                      /* strcmp */
#include "config.h"                      /* file paths and SMA windows */
#include "store/trades.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/*
 * process - run the engine over trades[0] .. trades[n-1]
 *
 * This is the main loop from stageB_manual.c, except each trade now comes
 * from the array instead of from next_trade().
 */
static void process(long long n, int short_win, int long_win, int print_table) {
    int signals_printed = 0;
    long long i;

    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();

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

        vwap_update(t->price, t->qty);

        if (ohlc_update(t->time, t->price) && print_table) {
            print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(),
                      ohlc_finished_low(), ohlc_finished_close(), ohlc_finished_n());
        }

        /* naive (V3) or running sum (V4), whichever sma_*.c was linked */
        sma_push(SHORT_SMA, t->price);
        sma_push(LONG_SMA, t->price);
        have_short = sma_ready(SHORT_SMA);
        have_long  = sma_ready(LONG_SMA);
        if (have_short) short_value = sma_value(SHORT_SMA);
        if (have_long)  long_value  = sma_value(LONG_SMA);

        if (have_short && have_long) {
            signal = crossover_update(short_value, long_value);
        }

        if (print_table) {
            print_trade_row(trade_no, t->price, t->qty, t->is_buy,
                            vwap_value(), have_short, short_value,
                            have_long, long_value, signal);
        } else if (signal != SIG_NONE && signals_printed < 10) {
            print_signal(signal, trade_no);
            signals_printed++;
        }
    }

    /* the last bar never gets "finished" by a new second, so print it here */
    if (ohlc_flush() && print_table) {
        print_bar_header();
        print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(),
                  ohlc_finished_low(), ohlc_finished_close(), ohlc_finished_n());
    }

    print_summary(vwap_value(), crossover_buys(), crossover_sells());
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

int main(int argc, char *argv[]) {
    /* "dev" as the first argument skips stress_full (valgrind runs) */
    int skip_stress = (argc > 1 && strcmp(argv[1], "dev") == 0);

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
