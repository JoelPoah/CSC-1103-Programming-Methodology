/*
 * stageA_running_sma.c - Stage A with a RUNNING-SUM SMA (ladder rung)
 *
 * Same program as stageA_manual.c (Version 0). The only change is the SMA
 * module: engine/running_sma.c keeps a running window sum, so each trade
 * costs one add + one subtract instead of re-adding the whole window.
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageA/stageA_running_sma.c stageA/engine/vwap.c stageA/engine/ohlc.c
 *       stageA/engine/running_sma.c stageA/engine/signal.c stageA/engine/report.c
 *       helper_function/trade_io.c -o stageA/stageA_running_sma
 * Run (from the project root):
 *   ./stageA/stageA_running_sma
 *
 * Expected results:
 *   test20.csv : identical to data/group_data/test20_expected.txt
 *   dev_1M.csv : final VWAP 76217.01291587, 2465 BUY, 2464 SELL (short=35, long=60)
 *   Version 0 gives 2509 BUY, 2469 SELL on dev_1M: the running sum drifts by
 *   tiny rounding errors, so on flat stretches short SMA == long SMA is no
 *   longer exactly true and some crossings are missed.
 *
 * No signals.dat / signals.png here: the plot in the report comes from
 * Version 0, and this program must not overwrite it.
 */
#include <stdio.h>
#include "../helper_function/trade_io.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/running_sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/* ---- file paths (relative to the project root) ---- */
#define TEST20_PATH   "data/group_data/test20.csv"
#define DEV_PATH      "data/group_data/dev_1M.csv"

/* ---- SMA windows ---- */
#define TEST_SHORT 3
#define TEST_LONG  5
#define DEV_SHORT  35
#define DEV_LONG   60

/*
 * run - process one whole trade file
 *
 * For every trade: read it, update each calculation, then print / write.
 * After the loop: finish the last OHLC bar and print the summary.
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

    /* STEP 1 - check the inputs: short < long <= MAX_WIN (engine/running_sma.h) */
    if (short_win >= long_win || long_win > MAX_WIN){
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

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
    int signal_printed = 0;
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

        /* 6c) SMA: running sum, one add + one subtract per window */
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

    /* STEP 7 - after the loop: print the last OHLC bar, close the files */
    if (ohlc_flush() && print_table == 1) {
        print_bar_header();
        print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
                  ohlc_finished_close(), ohlc_finished_n());
    }
    dat_close();
    close_trades();

    /* STEP 8 - final VWAP and BUY / SELL counts */
    print_summary(vwap_value(), crossover_buys(), crossover_sells());

    return 0;
}

int main(void) {
    /* PART A - correctness check on the 20-trade file */
    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1, 0, 0, NULL) != 0) {
        return 1;
    }

    /* PART B - the full dev file (no .dat slice, see the note at the top) */
    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0, 0, 0, NULL) != 0) {
        return 1;
    }

    return 0;
}
