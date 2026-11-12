/*
 * stageA_manual.c - Stage A written by hand, using the engine modules
 *
 * This is the group's VERSION 0: helper library (sscanf), double prices and
 * the NAIVE SMA in engine/sma.c (re-adds the whole window every trade).
 * Every ladder speedup, the rival bot and the race compare against it.
 *
 * One dataset per run, picked by the first argument:
 *   test20 - data/group_data/test20.csv, short=3,  long=5,  full table (correctness)
 *   dev    - data/group_data/dev_1M.csv, short=35, long=60, summary only
 *   stress - data/stress_full.csv,       short=35, long=60, summary only (benchmark)
 *
 * No timing inside: the group times every ladder version from outside with
 * benchmark/benchmark.c (now_ns() around the whole program), e.g. on "stress".
 * Because this program is timed, it writes NO signals.dat / signals.png (the
 * brief says never to write plot files in a version you time). The plot is
 * made by stageA_plot.c.
 *
 * Build (from the project root):
 *   make stageA
 *   or: gcc -O2 -Wall stageA/stageA_manual.c stageA/engine/vwap.c stageA/engine/ohlc.c
 *           stageA/engine/sma.c stageA/engine/signal.c stageA/engine/report.c
 *           helper_function/trade_io.c -o stageA/stageA_manual
 * Run (from the project root):
 *   ./stageA/stageA_manual test20
 *   ./stageA/stageA_manual dev
 *   ./stageA/stageA_manual stress
 * Or from inside stageA:
 *   ./stageA_manual test20   (dev / stress likewise)
 *
 * Check your answers against:
 *   data/group_data/test20_expected.txt  (table + signals for short=3, long=5)
 *   dev    (short=35, long=60): final VWAP 76217.01291587, 2509 BUY, 2469 SELL
 *   stress (short=35, long=60): 8,928,944 trades, final VWAP 76718.67569936,
 *                               20386 BUY, 20176 SELL
 */
#include <stdio.h>
#include <string.h>                      /* strcmp */
#ifdef _WIN32
#include <direct.h>
#define change_directory _chdir
#else
#include <unistd.h>
#define change_directory chdir
#endif
#include "../helper_function/trade_io.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/* ---- file paths (relative to the project root) ---- */
#define TEST20_PATH "data/group_data/test20.csv"
#define DEV_PATH    "data/group_data/dev_1M.csv"
#define STRESS_PATH "data/stress_full.csv"

/* ---- SMA windows ---- */
#define TEST_SHORT 3
#define TEST_LONG  5
#define DEV_SHORT  35          /* Group 06: dev_1M.csv and stress_full.csv */
#define DEV_LONG   60

/* Keep the dataset paths relative to the project root.
 * If launched inside stageA, move this process to its parent folder.
 * This does not change the working directory of the user's terminal.
 */
static int set_project_directory(void) {
    FILE *check = fopen(TEST20_PATH, "r");
    if (check != NULL) {
        fclose(check);
        return 1;
    }
    check = fopen("../" TEST20_PATH, "r");
    if (check != NULL) {
        fclose(check);
        if (change_directory("..") == 0) return 1;
        perror("Cannot switch to the project root");
        return 0;
    }
    fprintf(stderr, "Cannot find %s. Run from the project root or stageA folder.\n",
            TEST20_PATH);
    return 0;
}

/*
 * run - process one whole trade file
 *
 * Hint: think of the function as a loop with one body. For every trade:
 *   a) read the trade
 *   b) update each calculation
 *   c) decide what to print
 * After the loop: finish the last OHLC bar and print the summary.
 *
 *   path        - which CSV to read
 *   short_win   - SMA short window
 *   long_win    - SMA long window
 *   print_table - 1 = print one row per trade, 0 = summary only
 *
 * Returns the number of trades processed, or -1 on error.
 */
static long long run(const char *path, int short_win, int long_win, int print_table) {

    /* STEP 1 - check the inputs
     * Hint: the short window must be smaller than the long window, and
     * the long window must not exceed MAX_WIN (defined in engine/sma.h).
     */
    if (short_win >= long_win || long_win > MAX_WIN){
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return -1;
    }

    /* STEP 2 - open the input file
     * Hint: open_trades(path) returns 1 on success, 0 on failure.
     */
    if (!open_trades(path)){
        printf("cannot open %s\n", path);
        return -1;
    }

    /* STEP 3 - reset every calculation
     * Each module keeps its own variables, so just call its *_init function.
     * Hint: sma_init needs the window id (SHORT_SMA / LONG_SMA) and its size.
     */
    int signal_printed = 0;
    long long n = 0;
    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();

    /* STEP 4 - print the table header (only if print_table is 1) */
    if (print_table == 1){
        print_table_header();
    }

    /* STEP 5 - the main loop
     * Hint: while (next_trade()) { ... }  - inside it, use trade_number(),
     * trade_price(), trade_qty(), trade_time(), trade_is_buy().
     *
     * Inside the loop, in this order:
     *   5a) VWAP:  vwap_update(...)
     *   5b) OHLC:  ohlc_update(...)  - if it returns 1, print the finished
     *              bar (only when print_table is 1)
     *   5c) SMA:   push the price into both windows (SHORT_SMA and LONG_SMA)
     *   5d) If both SMAs are ready (sma_ready), get their values with sma_value
     *       and call crossover_update to get SIG_BUY / SIG_SELL / SIG_NONE.
     *       Remember: the short and long values may not exist for the first
     *       few trades - use the "have_*" flags so you print "-" in that case.
     *   5e) If print_table is 1, print the row (print_trade_row).
     *       Otherwise, print only the first 10 signals.
     */
    while (next_trade()){
        int signal = SIG_NONE;
        double short_sma_value = 0.0, long_sma_value = 0.0;
        n++;

        vwap_update(trade_price(), trade_qty());
        if (ohlc_update(trade_time(), trade_price()) && print_table == 1){
            print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
            ohlc_finished_close(), ohlc_finished_n());
        }
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

        if (have_sma_short && have_sma_long){
            signal = crossover_update(short_sma_value, long_sma_value);
        }

        if (print_table == 1){
                print_trade_row(trade_number(), trade_price(), trade_qty(), trade_is_buy(),
                vwap_value(), have_sma_short, short_sma_value, have_sma_long,
                long_sma_value, signal);
        }
        else if (signal != SIG_NONE && signal_printed < 10){
                print_signal(signal, trade_number());
                signal_printed++;
        }
    }

    /* STEP 6 - after the loop
     * Hint: print the last OHLC bar (only when print_table is 1), then call
     * close_trades().
     */
    if (ohlc_flush() && print_table == 1) {
        print_bar_header();
        print_bar(ohlc_finished_sec(), ohlc_finished_open(), ohlc_finished_high(), ohlc_finished_low(),
                  ohlc_finished_close(), ohlc_finished_n());
    }
    close_trades();

    /* STEP 7 - print the summary (print_summary) with the final VWAP and the
     * BUY / SELL counts (crossover_buys / crossover_sells).
     */
    print_summary(vwap_value(), crossover_buys(), crossover_sells());

    return n;
}

/* print_usage - shown when the dataset argument is missing or wrong */
static void print_usage(void) {
    printf("usage: ./stageA/stageA_manual test20 | dev | stress\n");
    printf("  test20 : %s (short=%d, long=%d), prints every trade\n", TEST20_PATH, TEST_SHORT, TEST_LONG);
    printf("  dev    : %s (short=%d, long=%d)\n", DEV_PATH, DEV_SHORT, DEV_LONG);
    printf("  stress : %s (short=%d, long=%d)\n", STRESS_PATH, DEV_SHORT, DEV_LONG);
}

int main(int argc, char *argv[]) {
    // we need exactly one word after the program name: test20, dev or stress
    if (argc != 2){
        print_usage();
        return 1;
    }

    if (!set_project_directory()) return 1;

    const char *dataset = argv[1];
    const char *path;
    int short_window;
    int long_window;
    int print_table;

    // pick the file, the SMA windows and how much to print for this dataset
    if (strcmp(dataset, "test20") == 0){
        path = TEST20_PATH;
        short_window = TEST_SHORT;
        long_window = TEST_LONG;
        print_table = 1;            // only 20 trades, so print every one
    }
    else if (strcmp(dataset, "dev") == 0){
        path = DEV_PATH;
        short_window = DEV_SHORT;
        long_window = DEV_LONG;
        print_table = 0;            // 1M trades, summary only
    }
    else if (strcmp(dataset, "stress") == 0){
        path = STRESS_PATH;
        short_window = DEV_SHORT;
        long_window = DEV_LONG;
        print_table = 0;            // full day, summary only (this is the benchmark)
    }
    else {
        printf("unknown dataset: %s\n", dataset);
        print_usage();
        return 1;
    }

    printf("=== %s (short=%d, long=%d) ===\n", path, short_window, long_window);

    long long trade_count = run(path, short_window, long_window, print_table);

    if (trade_count < 0){
        return 1;                   // run() already printed what went wrong
    }

    printf("Trades: %lld\n", trade_count);
    return 0;
}
