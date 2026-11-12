/*
 * Stage A+: Group 06 engine driven by our own fast CSV reader
 * (read_trade, in fast_reader.c).
 * Build from the project root. Run from the root or the stageA_plus folder.
 *
 * Build using the engine modules:
 *   gcc -std=c11 -O2 -Wall -Wextra stageA_plus/stageA_plus.c stageA_plus/fast_reader.c stageA_plus/engine/vwap.c stageA_plus/engine/ohlc.c stageA_plus/engine/sma.c stageA_plus/engine/signal.c stageA_plus/engine/report.c -o stageA_plus/stageA_plus
 *
 * Run:  ./stageA_plus/stageA_plus                  test20 table, then dev summary
 *       ./stageA_plus/stageA_plus --test          test20 only (windows 3/5)
 *       ./stageA_plus/stageA_plus --dev           dev_1M only (windows 35/60)
 *       ./stageA_plus/stageA_plus --stress       stress_full summary (35/60)
 * Windows executable names have .exe. Also run these commands on the Pi.
 * No timing measurements or plot files are produced by this program.
 */
#include <stdio.h>
#include <string.h>
#include "fast_reader.h"
#include "engine/vwap.h"
#include "engine/ohlc.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

#define TEST_PATH "data/group_data/test20.csv"
#define DEV_PATH "data/group_data/dev_1M.csv"
#define STRESS_PATH "data/stress_full.csv"
#define SHORT_WINDOW 35
#define LONG_WINDOW 60

/* Read trades using our parser and update the engine modules. */
static int run(const char *path, int short_win, int long_win, int table) {
    FILE *fp = NULL;
    char parent_path[256];
    long long timestamp = 0, count = 0, bars = 0;
    double price = 0.0, qty = 0.0;
    int is_buy = 0, status;

    if (short_win < 1 || short_win >= long_win || long_win > MAX_WIN) {
        fprintf(stderr, "Need 1 <= short < long <= %d\n", MAX_WIN);
        return 1;
    }
    fp = fopen(path, "r");
    /* Default datasets are relative to the root. When launched inside
     * stageA_plus, try its parent instead. User-supplied paths stay relative
     * to the current working folder, as normal terminal paths do.
     */
    if (fp == NULL && (strcmp(path, TEST_PATH) == 0 ||
                       strcmp(path, DEV_PATH) == 0 ||
                       strcmp(path, STRESS_PATH) == 0)) {
        snprintf(parent_path, sizeof parent_path, "../%s", path);
        fp = fopen(parent_path, "r");
        if (fp != NULL) path = parent_path;
    }
    if (fp == NULL) { perror(path); return 1; }
    /* Reset the modules before processing each file. */
    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();
    if (table) print_table_header();
    for (;;) {
        int signal = SIG_NONE;
        int have_short, have_long;
        double sma_s = 0.0, sma_l = 0.0;
        status = read_trade(fp, &timestamp, &price, &qty, &is_buy);
        if (status != 1) break;
        count++;
        vwap_update(price, qty);
        if (ohlc_update(timestamp, price)) {
            bars++;
            if (table) {
                print_bar(ohlc_finished_sec(), ohlc_finished_open(),
                          ohlc_finished_high(), ohlc_finished_low(),
                          ohlc_finished_close(), ohlc_finished_n());
            }
        }

        sma_push(SHORT_SMA, price);
        sma_push(LONG_SMA, price);
        have_short = sma_ready(SHORT_SMA);
        have_long = sma_ready(LONG_SMA);
        if (have_short) sma_s = sma_value(SHORT_SMA);
        if (have_long) sma_l = sma_value(LONG_SMA);
        if (have_short && have_long) {
            signal = crossover_update(sma_s, sma_l);
        }
        if (table) {
            print_trade_row(count, price, qty, is_buy, vwap_value(),
                            have_short, sma_s, have_long, sma_l, signal);
        }
    }
    /* The last OHLC bar has no following trade to close it. */
    if (ohlc_flush()) bars++;
    if (fclose(fp) != 0) status = -1;
    if (status < 0) {
        fprintf(stderr, "%s: invalid row or I/O error after trade %lld\n", path, count);
        return 1;
    }
    if (count == 0) { fprintf(stderr, "%s: no trades\n", path); return 1; }
    if (table) print_bar_header();
    printf("Last OHLC bar: second, open, high, low, close, trades\n");
    print_bar(ohlc_finished_sec(), ohlc_finished_open(),
              ohlc_finished_high(), ohlc_finished_low(),
              ohlc_finished_close(), ohlc_finished_n());
    printf("Trades: %lld; OHLC bars: %lld\n", count, bars);
    print_summary(vwap_value(), crossover_buys(), crossover_sells());
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--test") == 0)
        return run(TEST_PATH, 3, 5, 1);
    if (argc == 2 && strcmp(argv[1], "--dev") == 0)
        return run(DEV_PATH, SHORT_WINDOW, LONG_WINDOW, 0);
    if (argc == 2 && strcmp(argv[1], "--stress") == 0)
        return run(STRESS_PATH, SHORT_WINDOW, LONG_WINDOW, 0);
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [--test | --dev | --stress]\n", argv[0]);
        return 1;
    }
    printf("=== test20.csv (3/5); reader: own digit parser ===\n");
    if (run(TEST_PATH, 3, 5, 1)) return 1;
    printf("\n=== dev_1M.csv (35/60) ===\n");
    return run(DEV_PATH, SHORT_WINDOW, LONG_WINDOW, 0);
}
