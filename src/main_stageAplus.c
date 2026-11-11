/*
 * main_stageAplus.c - Stage A+ streaming engine with our OWN reader
 *
 * The same loop as main_stageA.c (Version 0). The only change is where the
 * trades come from: the helper library is replaced by our read_trade(),
 * which hands back the time, price, qty and side through pointers.
 *
 * Built by the Makefile as:
 *   bin/v1_stageAplus_own_reader    this file + reader/read_trade_sscanf.c
 *   bin/v2_stageAplus_fast_parser   this file + reader/read_trade_fast.c
 *
 * Run (from the project root):
 *   ./bin/v2_stageAplus_fast_parser test20 | dev | stress
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "reader/read_trade.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/*
 * run - process one whole trade file
 *
 *   path        - which CSV to read
 *   short_win   - SMA short window
 *   long_win    - SMA long window
 *   print_table - 1 = print one row per trade, 0 = summary only
 *
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t start_ns, end_ns;
    int signal_printed = 0;

    /* the current trade: read_trade fills these in through pointers */
    long long trade_no = 0;      /* 1 for the first trade, 2 for the next... */
    long long time_us;
    double price, qty;
    int is_buy;

    /* STEP 1 - check the inputs: short < long <= MAX_WIN (engine/sma.h) */
    if (short_win >= long_win || long_win > MAX_WIN){
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    start_ns = now_ns();

    /* STEP 2 - open the input file ourselves (fopen returns NULL on failure) */
    FILE *trades_file = fopen(path, "r");
    if (trades_file == NULL){
        printf("cannot open %s\n", path);
        return 1;
    }

    /* STEP 3 - reset every calculation (each module keeps its own variables) */
    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();

    /* STEP 4 - print the table header */
    if (print_table == 1){
        print_table_header();
    }

    /* STEP 5 - the main loop, one trade at a time
     * &time_us means "the address of time_us": read_trade writes the
     * value straight into our variable. */
    while (read_trade(trades_file, &time_us, &price, &qty, &is_buy)){
        int signal = SIG_NONE;
        double short_sma_value = 0.0, long_sma_value = 0.0;
        trade_no++;

        /* 5a) VWAP */
        vwap_update(price, qty);

        /* 5b) OHLC: print the bar this trade just finished */
        if (ohlc_update(time_us, price) && print_table == 1){
            print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
            ohlc_finished_close(), ohlc_finished_n());
        }

        /* 5c) SMA: push the price into both windows */
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

        /* 5d) crossover signal, once both SMAs exist */
        if (have_sma_short && have_sma_long){
            signal = crossover_update(short_sma_value, long_sma_value);
        }

        /* 5e) print the row, or only the first 10 signals in summary mode */
        if (print_table == 1){
                print_trade_row(trade_no, price, qty, is_buy,
                vwap_value(), have_sma_short, short_sma_value, have_sma_long,
                long_sma_value, signal);
        }
        else if (signal != SIG_NONE && signal_printed < 10){
                print_signal(signal, trade_no);
                signal_printed++;
        }
    }

    /* STEP 6 - after the loop: close the file and stop the clock */
    fclose(trades_file);
    end_ns = now_ns();

    /* STEP 7 - final VWAP and BUY / SELL counts, then the last OHLC bar */
    print_summary(vwap_value(), crossover_buys(), crossover_sells());
    if (ohlc_flush() && print_table == 1) {
        print_bar_header();
        print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
                  ohlc_finished_close(), ohlc_finished_n());
    }

    /* STEP 8 - how long it took */
    print_timing(trade_no, (end_ns - start_ns) / 1e6);

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

    /* MODE test20 - correctness check on the 20-trade file */
    if (strcmp(argv[1], "test20") == 0) {
        printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
        return run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1);
    }

    /* MODE dev - the 1 million trade development file */
    if (strcmp(argv[1], "dev") == 0) {
        printf("=== dev_1M.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(DEV_PATH, GROUP_SHORT, GROUP_LONG, 0);
    }

    /* MODE stress - the full day, used for every benchmark */
    if (strcmp(argv[1], "stress") == 0) {
        printf("=== stress_full.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(STRESS_PATH, GROUP_SHORT, GROUP_LONG, 0);
    }

    print_usage(argv[0]);
    return 1;
}
