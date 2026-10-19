/*
 * stageB_manual.c - Stage B written by hand, using the engine modules
 *
 * This is a guided skeleton. Each TODO tells you WHAT to do and in WHICH
 * order; you write the actual C code yourself.
 *
 * Build (from the project root):
 *   gcc -O2 -Wall stageB/stageB_manual.c stageB/engine/vwap.c stageB/engine/ohlc.c
 *       stageB/engine/sma.c stageB/engine/signal.c stageB/engine/report.c
 *       helper_function/trade_io.c -o stageB/stageB_manual
 * Run (from the project root):
 *   ./stageB/stageB_manual
 *
 * Check your answers against:
 *   data/group_data/test20_expected.txt  (table + signals for short=3, long=5)
 *   the dev_1M numbers from the old program: final VWAP 76217.01291587,
 *   2465 BUY, 2464 SELL (short=35, long=60)
 */
#include <stdio.h>
#include <stdlib.h>
#include "../helper_function/trade_io.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/* ---- file paths (relative to the project root) ---- */
#define TEST20_PATH   "data/group_data/test20.csv"
#define DEV_PATH      "data/group_data/dev_1M.csv"
#define SIGNALS_DAT   "stageB/signals.dat"
#define PLOT_SCRIPT "stageB/plot_signals.gnuplot"

/* ---- SMA windows ---- */
#define TEST_SHORT 3
#define TEST_LONG  5
#define DEV_SHORT  35
#define DEV_LONG   60

/* ---- slice written to signals.dat ---- */
#define SLICE_START 1
#define SLICE_LEN   2000

/*
 * run - process one whole trade file
 *
 * Hint: think of the function as a loop with one body. For every trade:
 *   a) read the trade
 *   b) update each calculation
 *   c) decide what to print / write
 * After the loop: finish the last OHLC bar and print the summary.
 *
 * Parameters you will probably need:
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

    /* STEP 1 - check the inputs
     * Hint: the short window must be smaller than the long window, and
     * the long window must not exceed MAX_WIN (defined in engine/sma.h).
     * Print a message and return 1 if not.
     */
    /* TODO */
    if (short_win >= long_win || long_win > MAX_WIN){
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    /* STEP 2 - open the input file
     * Hint: open_trades(path) returns 1 on success, 0 on failure.
     */
    /* TODO */
    
    if (!open_trades(path)){
        printf("cannot open %s\n", path); 
        return 1; // fail
    }


    /* STEP 3 - open the .dat output (only if dat_path is not NULL)
     * Hint: dat_open(path) returns a FILE * or NULL. Check for NULL.
     */
    /* TODO */
    FILE *dat = NULL;
    if (dat_path != NULL ){
        dat = dat_open(dat_path);
        if (dat == NULL){
            printf("Failed to open .dat %s\n", dat_path);
            close_trades();
            return 1;
        }
    }
    /* STEP 4 - set up the state objects
     * Create one Vwap, one OhlcBar, two Sma (short and long) and one Crossover.
     * Call the matching *_init function for each one.
     * Hint: an Sma needs its window size passed to sma_init.
     */
    /* TODO */
    Vwap vwap;
    OhlcBar bar, finished;
    Sma short_sma, long_sma;
    Crossover crossover_signal;
    int signal_printed = 0;
    vwap_init(&vwap);
    ohlc_init(&bar);
    ohlc_init(&finished);
    sma_init(&short_sma, short_win);
    sma_init(&long_sma, long_win); 
    crossover_init(&crossover_signal);

    /* STEP 5 - print the table header (only if print_table is 1) */
    /* TODO */
    if (print_table == 1){
        print_table_header();
    }
    

    /* STEP 6 - the main loop
     * Hint: while (next_trade()) { ... }  - inside it, use trade_number(),
     * trade_price(), trade_qty(), trade_time(), trade_is_buy().
     *
     * Inside the loop, in this order:
     *   6a) VWAP:  vwap_update(...)
     *   6b) OHLC:  ohlc_update(...)  - if it returns 1, print the finished
     *              bar (only when print_table is 1)
     *   6c) SMA:   push the price into both Sma objects
     *   6d) If both SMAs are ready (sma_ready), get their values with sma_value
     *       and call crossover_update to get SIG_BUY / SIG_SELL / SIG_NONE.
     *       Remember: the short and long values may not exist for the first
     *       few trades - use the "have_*" flags so you print "-" in that case.
     *   6e) If print_table is 1, print the row (print_trade_row).
     *       Otherwise, print only the first 10 signals.
     *   6f) If the trade number is inside [slice_from, slice_to] and the
     *       long SMA is ready, write a row to the .dat file.
     */
    /* TODO */
    while (next_trade()){
        int signal = SIG_NONE;
        double short_sma_value = 0.0, long_sma_value = 0.0;
        vwap_update(&vwap, trade_price(), trade_qty());
        if (ohlc_update(&bar, trade_time(), trade_price(), &finished) && print_table == 1){
            print_bar(finished.sec, finished.open, finished.high, finished.low,
            finished.close, finished.n);
        }
        sma_push(&short_sma, trade_price());
        sma_push(&long_sma, trade_price());

        int have_sma_short = sma_ready(&short_sma);
        int have_sma_long = sma_ready(&long_sma);
        if (have_sma_short){
            short_sma_value = sma_value(&short_sma);
        }
        
        if (have_sma_long){
            long_sma_value = sma_value(&long_sma);
        }

        if (have_sma_short && have_sma_long){
            signal = crossover_update(&crossover_signal, short_sma_value, long_sma_value);
        }

        if (print_table == 1){
                print_trade_row(trade_number(), trade_price(), trade_qty(), trade_is_buy(),
                vwap_value(&vwap), have_sma_short, short_sma_value, have_sma_long,
                long_sma_value, signal);
        }
        else if (signal != SIG_NONE && signal_printed < 10){
                printf("  signal %-4s at trade %lld\n", signal_name(signal), trade_number());
                signal_printed++;
        }    

        // trade number
        if (dat !=NULL && have_sma_long && (trade_number() >= slice_from) && (trade_number() <= slice_to)){
            //write a row to .dat file
            dat_write_row(dat, trade_number(), trade_price(), short_sma_value, long_sma_value, signal);
        } 
    }




    /* STEP 7 - after the loop
     * Hint: print the last OHLC bar (only when print_table is 1), then close
     * the .dat file and call close_trades().
     */
    if (print_table == 1 && bar.active) {
        print_bar_header();
        print_bar(bar.sec, bar.open, bar.high, bar.low,
                  bar.close, bar.n);
    }
    dat_close(dat);
    close_trades();

    /* STEP 8 - print the summary (print_summary) with the final VWAP and the
     * BUY / SELL counts from the Crossover struct.
     */
    print_summary(vwap_value(&vwap), crossover_signal.buys,
                  crossover_signal.sells);
    
    return 0;

}

int main(void) {
    /* PART A - correctness check on the 20-trade file
     * Hint: call run(...) with TEST20_PATH, TEST_SHORT, TEST_LONG, print_table=1,
     * no slice (0, 0) and no .dat file (NULL).
     */
    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1, 0, 0, NULL) != 0) {
        return 1;
    }

    /* PART B - the full dev file
     * Hint: same call as Part A but with DEV_PATH, DEV_SHORT, DEV_LONG,
     * print_table=0, the slice numbers and SIGNALS_DAT.
     */
    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0,
            SLICE_START, SLICE_START + SLICE_LEN - 1, SIGNALS_DAT) != 0) {
        return 1;
    }

    /* PART C - draw signals.png */
    printf("\nRunning gnuplot on %s ...\n", PLOT_SCRIPT);
    if (system("gnuplot " PLOT_SCRIPT) != 0) {
        printf("gnuplot failed or is not installed; signals.dat is still written.\n");
    }

    return 0;
}
