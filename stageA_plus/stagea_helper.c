/*
 * Group 06 engine using the original trade_io helper reader (sscanf).
 * Uses the same engine modules and dataset choices as stageA_plus.c.
 *
 * Build from the project root:
 * gcc -std=c11 -O2 -Wall -Wextra stageA_plus/stagea_helper.c stageA_plus/engine/vwap.c stageA_plus/engine/ohlc.c stageA_plus/engine/sma.c stageA_plus/engine/signal.c stageA_plus/engine/report.c helper_function/trade_io.c -o stageA_plus/stagea_helper
 *
 * Run from the root: ./stageA_plus/stagea_helper [--test | --dev | --stress]
 * Run inside stageA_plus: ./stagea_helper [--test | --dev | --stress]
 * On Windows, the executable has the .exe extension.
 * No option: test20 table followed by development summary.
 * No custom parser, benchmark functions or plot generation.
 */
#include <stdio.h>
#include <string.h>
#include "../helper_function/trade_io.h"
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

static int run(const char *path, int short_win, int long_win, int show_table) {
    char parent_path[256];
    long long count = 0, bars = 0;

    if (short_win < 1 || short_win >= long_win || long_win > MAX_WIN) {
        fprintf(stderr, "Need 1 <= short < long <= %d\n", MAX_WIN);
        return 1;
    }

    /* Find the dataset before handing its path to the helper reader. */
    FILE *check = fopen(path, "r");
    if (check == NULL) {
        snprintf(parent_path, sizeof parent_path, "../%s", path);
        check = fopen(parent_path, "r");
        if (check != NULL) {
            path = parent_path;
        }
    }
    if (check == NULL) {
        perror(path);
        return 1;
    }
    fclose(check);
    if (!open_trades(path)) {
        return 1;
    }

    vwap_init();
    ohlc_init();
    sma_init(SHORT_SMA, short_win);
    sma_init(LONG_SMA, long_win);
    crossover_init();
    if (show_table) {
        print_table_header();
    }

    /* The helper reads each row. Getters return its converted values. */
    while (next_trade()) {
        long long timestamp = trade_time();
        double price = trade_price();
        double qty = trade_qty();
        int is_buy = trade_is_buy();
        int signal = SIG_NONE;
        double short_sma = 0.0, long_sma = 0.0;
        count++;

        vwap_update(price, qty);
        if (ohlc_update(timestamp, price)) {
            bars++;
            if (show_table) {
                print_bar(ohlc_finished_sec(), ohlc_finished_open(),
                          ohlc_finished_high(), ohlc_finished_low(),
                          ohlc_finished_close(), ohlc_finished_n());
            }
        }
        sma_push(SHORT_SMA, price);
        sma_push(LONG_SMA, price);
        int have_short = sma_ready(SHORT_SMA);
        int have_long = sma_ready(LONG_SMA);
        if (have_short) {
            short_sma = sma_value(SHORT_SMA);
        }
        if (have_long) {
            long_sma = sma_value(LONG_SMA);
        }
        if (have_short && have_long) {
            signal = crossover_update(short_sma, long_sma);
        }
        if (show_table) {
            print_trade_row(count, price, qty, is_buy, vwap_value(),
                            have_short, short_sma, have_long, long_sma, signal);
        }
    }
    /* The helper skips malformed rows and returns 0 at end of input.
     * Its API does not distinguish an I/O error from normal EOF.
     */
    close_trades();
    if (count == 0) {
        fprintf(stderr, "%s: no trades\n", path);
        return 1;
    }
    if (ohlc_flush()) {
        bars++;
    }
    if (show_table) {
        print_bar_header();
    }
    printf("Last OHLC bar: second, open, high, low, close, trades\n");
    print_bar(ohlc_finished_sec(), ohlc_finished_open(),
              ohlc_finished_high(), ohlc_finished_low(),
              ohlc_finished_close(), ohlc_finished_n());
    printf("Trades: %lld; OHLC bars: %lld\n", count, bars);
    print_summary(vwap_value(), crossover_buys(), crossover_sells());
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        return run(TEST_PATH, 3, 5, 1);
    }
    if (argc == 2 && strcmp(argv[1], "--dev") == 0) {
        return run(DEV_PATH, SHORT_WINDOW, LONG_WINDOW, 0);
    }
    if (argc == 2 && strcmp(argv[1], "--stress") == 0) {
        return run(STRESS_PATH, SHORT_WINDOW, LONG_WINDOW, 0);
    }
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [--test | --dev | --stress]\n", argv[0]);
        return 1;
    }
    printf("=== test20.csv (3/5); reader: original helper ===\n");
    if (run(TEST_PATH, 3, 5, 1) != 0) {
        return 1;
    }
    printf("\n=== dev_1M.csv (35/60) ===\n");
    return run(DEV_PATH, SHORT_WINDOW, LONG_WINDOW, 0);
}
