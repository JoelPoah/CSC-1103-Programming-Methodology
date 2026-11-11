/*
 * main_stageA.c - Stage A streaming engine (the group's VERSION 0)
 *
 * Reads one trade at a time with the helper library (sscanf), uses double
 * prices and the engine/ modules. Every speedup on the ladder, the rival
 * bot and the race compare against this program.
 *
 * Built by the Makefile as:
 *   bin/v0_stageA_naive_sma    this file + engine/sma_naive.c
 *
 * Run (from the project root):
 *   ./bin/v0_stageA_naive_sma test20    table to compare with test20_expected.txt
 *   ./bin/v0_stageA_naive_sma dev       dev_1M.csv summary + time
 *   ./bin/v0_stageA_naive_sma stress    stress_full.csv summary + time (benchmark)
 *   ./bin/v0_stageA_naive_sma plot      dev_1M.csv slice -> signals.dat + signals.png
 *
 * Only "plot" writes a file. The timed modes never do, because writing a
 * file is slow and would distort the measurement.
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../helper_function/trade_io.h"
#include "config.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/*
 * run - process one whole trade file
 *
 * For every trade: read it, update each calculation, then print / write.
 * After the loop: print the summary and the last OHLC bar.
 *
 *   path        - which CSV to read
 *   short_win   - SMA short window
 *   long_win    - SMA long window
 *   print_table - 1 = print one row per trade, 0 = summary only
 *   slice_from / slice_to - trade numbers to write to the .dat file
 *   dat_path    - output file name, or NULL for no .dat file
 *
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table,
               long long slice_from, long long slice_to, const char *dat_path) {
    uint64_t start_ns, end_ns;
    int signal_printed = 0;

    /* STEP 1 - check the inputs: short < long <= MAX_WIN (engine/sma.h) */
    if (short_win >= long_win || long_win > MAX_WIN){
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    /* the clock starts before the file is opened: reading is part of the work */
    start_ns = now_ns();

    /* STEP 2 - open the input file (open_trades returns 1 on success) */
    if (!open_trades(path)){
        printf("cannot open %s\n", path);
        return 1;
    }

    /* STEP 3 - open the .dat output, only if a path was given */
    if (dat_path != NULL){
        if (!dat_open(dat_path)){
            printf("Failed to open .dat %s\n", dat_path);
            close_trades();
            return 1;
        }
    }

    /* STEP 4 - reset every calculation (each module keeps its own variables) */
    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();

    /* STEP 5 - print the table header */
    if (print_table == 1){
        print_table_header();
    }

    /* STEP 6 - the main loop, one trade at a time */
    while (next_trade()){
        int signal = SIG_NONE;
        double short_sma_value = 0.0, long_sma_value = 0.0;

        /* 6a) VWAP */
        vwap_update(trade_price(), trade_qty());

        /* 6b) OHLC: print the bar this trade just finished */
        if (ohlc_update(trade_time(), trade_price()) && print_table == 1){
            print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
            ohlc_finished_close(), ohlc_finished_n());
        }

        /* 6c) SMA: push the price into both windows */
        sma_push(SHORT_SMA, trade_price());
        sma_push(LONG_SMA, trade_price());

        int have_sma_short = sma_ready(SHORT_SMA);
        int have_sma_long = sma_ready(LONG_SMA);
        if (have_sma_short){
            short_sma_value = sma_value(SHORT_SMA);
        }
        if (have_sma_long){
            long_sma_value = sma_value(LONG_SMA);
        }

        /* 6d) crossover signal, once both SMAs exist */
        if (have_sma_short && have_sma_long){
            signal = crossover_update(short_sma_value, long_sma_value);
        }

        /* 6e) print the row, or only the first 10 signals in summary mode */
        if (print_table == 1){
                print_trade_row(trade_number(), trade_price(), trade_qty(), trade_is_buy(),
                vwap_value(), have_sma_short, short_sma_value, have_sma_long,
                long_sma_value, signal);
        }
        else if (signal != SIG_NONE && signal_printed < 10){
                print_signal(signal, trade_number());
                signal_printed++;
        }

        /* 6f) .dat row for the slice (dat_write_row does nothing if no file is open) */
        if (have_sma_long && (trade_number() >= slice_from) && (trade_number() <= slice_to)){
            dat_write_row(trade_number(), trade_price(), short_sma_value, long_sma_value, signal);
        }
    }

    /* STEP 7 - after the loop: stop the clock, then close the files */
    long long trades_done = trade_number();
    dat_close();
    close_trades();
    end_ns = now_ns();

    /* STEP 8 - final VWAP and BUY / SELL counts, then the last OHLC bar
     * (same order as data/group_data/test20_expected.txt) */
    print_summary(vwap_value(), crossover_buys(), crossover_sells());
    if (ohlc_flush() && print_table == 1) {
        print_bar_header();
        print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
                  ohlc_finished_close(), ohlc_finished_n());
    }

    /* STEP 9 - how long it took */
    print_timing(trades_done, (end_ns - start_ns) / 1e6);

    return 0;
}

/* print_usage - remind the user which modes exist */
static void print_usage(const char *program) {
    printf("usage: %s test20 | dev | stress | plot\n", program);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* MODE test20 - correctness check on the 20-trade file */
    if (strcmp(argv[1], "test20") == 0) {
        printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
        return run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1, 0, 0, NULL);
    }

    /* MODE dev - the 1 million trade development file */
    if (strcmp(argv[1], "dev") == 0) {
        printf("=== dev_1M.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(DEV_PATH, GROUP_SHORT, GROUP_LONG, 0, 0, 0, NULL);
    }

    /* MODE stress - the full day, used for every benchmark */
    if (strcmp(argv[1], "stress") == 0) {
        printf("=== stress_full.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(STRESS_PATH, GROUP_SHORT, GROUP_LONG, 0, 0, 0, NULL);
    }

    /* MODE plot - write the signals.dat slice, then draw signals.png */
    if (strcmp(argv[1], "plot") == 0) {
        printf("=== dev_1M.csv slice for the plot (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        if (run(DEV_PATH, GROUP_SHORT, GROUP_LONG, 0,
                SLICE_START, SLICE_START + SLICE_LEN - 1, SIGNALS_DAT) != 0) {
            return 1;
        }
        printf("\nRunning gnuplot on %s ...\n", PLOT_SCRIPT);
        if (system("gnuplot " PLOT_SCRIPT) != 0) {
            printf("gnuplot failed or is not installed; %s is still written.\n", SIGNALS_DAT);
        }
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
