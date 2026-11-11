/*
 * main_stageB_inmem.c - Stage B in-memory engine (double prices)
 *
 * The same calculations as Stage A+, but in two separate phases:
 *   1) LOAD:    read the whole CSV into the global trades[] array
 *   2) PROCESS: loop over the array (no file reading here)
 * Each phase is timed with now_ns(), so we can see how much of the run
 * was file reading and how much was the actual maths.
 *
 * Built by the Makefile as:
 *   bin/v3_stageB_in_memory     this file + engine/sma_naive.c
 *   bin/v4_stageB_running_sma   this file + engine/sma_running.c
 * Both use the fast parser from Stage A+ to load the file.
 *
 * Run (from the project root):
 *   ./bin/v4_stageB_running_sma test20 | dev | stress
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "store/trades.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/*
 * process - run the engine over trades[0] .. trades[n-1]
 *
 * This is the main loop from Stage A+, except each trade now comes from
 * the array instead of from the file.
 */
static void process(long long n, int short_win, int long_win, int print_table) {
    int signal_printed = 0;
    long long i;

    /* STEP 1 - reset every calculation */
    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();

    /* STEP 2 - print the table header */
    if (print_table == 1){
        print_table_header();
    }

    /* STEP 3 - the main loop, one array element at a time */
    for (i = 0; i < n; i++){
        long long trade_no = i + 1;          /* trade numbers start at 1 */
        double price = trades[i].price;
        int signal = SIG_NONE;
        double short_sma_value = 0.0, long_sma_value = 0.0;

        /* 3a) VWAP */
        vwap_update(price, trades[i].qty);

        /* 3b) OHLC: print the bar this trade just finished */
        if (ohlc_update(trades[i].time, price) && print_table == 1){
            print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
            ohlc_finished_close(), ohlc_finished_n());
        }

        /* 3c) SMA: push the price into both windows */
        sma_push(SHORT_SMA, price);
        sma_push(LONG_SMA, price);

        int have_sma_short = sma_ready(SHORT_SMA);
        int have_sma_long = sma_ready(LONG_SMA);
        if (have_sma_short){
            short_sma_value = sma_value(SHORT_SMA);
        }
        if (have_sma_long){
            long_sma_value = sma_value(LONG_SMA);
        }

        /* 3d) crossover signal, once both SMAs exist */
        if (have_sma_short && have_sma_long){
            signal = crossover_update(short_sma_value, long_sma_value);
        }

        /* 3e) print the row, or only the first 10 signals in summary mode */
        if (print_table == 1){
                print_trade_row(trade_no, price, trades[i].qty, trades[i].is_buy,
                vwap_value(), have_sma_short, short_sma_value, have_sma_long,
                long_sma_value, signal);
        }
        else if (signal != SIG_NONE && signal_printed < 10){
                print_signal(signal, trade_no);
                signal_printed++;
        }
    }
}

/*
 * run - load one file, then process it, timing both phases
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t start_ns, loaded_ns, end_ns;
    long long n;

    if (short_win >= long_win || long_win > MAX_WIN){
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    /* PHASE 1 - load */
    start_ns = now_ns();
    n = load_trades(path);
    loaded_ns = now_ns();
    if (n < 0){
        return 1;
    }

    /* PHASE 2 - process */
    process(n, short_win, long_win, print_table);
    end_ns = now_ns();

    /* summary, last OHLC bar, then the timings */
    print_summary(vwap_value(), crossover_buys(), crossover_sells());
    if (ohlc_flush() && print_table == 1) {
        print_bar_header();
        print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
                  ohlc_finished_close(), ohlc_finished_n());
    }
    print_timing(n, (end_ns - start_ns) / 1e6);
    print_load_process((loaded_ns - start_ns) / 1e6, (end_ns - loaded_ns) / 1e6);
    printf("Trade array: %lld bytes\n", (long long)sizeof trades);   /* always full size */

    return 0;
}

/* print_usage - remind the user which modes exist */
static void print_usage(const char *program) {
    printf("usage: %s test20 | dev | stress\n", program);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "test20") == 0) {
        printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
        return run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1);
    }

    if (strcmp(argv[1], "dev") == 0) {
        printf("=== dev_1M.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(DEV_PATH, GROUP_SHORT, GROUP_LONG, 0);
    }

    if (strcmp(argv[1], "stress") == 0) {
        printf("=== stress_full.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(STRESS_PATH, GROUP_SHORT, GROUP_LONG, 0);
    }

    print_usage(argv[0]);
    return 1;
}
