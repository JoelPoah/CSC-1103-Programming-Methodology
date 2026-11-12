/*
 * display.c - coloured terminal output (see display.h)
 */
#define __USE_MINGW_ANSI_STDIO 1     /* Windows: makes %lld work reliably */
#include <stdio.h>
#include "display.h"
#include "../../stageB/engine/signal.h"   /* SIG_BUY, SIG_SELL */

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

/* display_init
 * What:    Windows only: let the console understand ANSI colour codes.
 *          Linux / macOS / Pi terminals already do.
 */
void display_init(void) {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode)) {
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

/* display_clock
 * What:    time of day of a trade, from its microsecond timestamp
 * Steps:   seconds since 1970 -> seconds into the day -> HH:MM:SS (UTC)
 */
void display_clock(long long ts_us, char *out) {
    long long day_sec = (ts_us / 1000000LL) % 86400LL;
    snprintf(out, 16, "%02lld:%02lld:%02lld",
             day_sec / 3600, (day_sec / 60) % 60, day_sec % 60);
}

/* display_signal
 * What:    one line per signal, green for BUY and red for SELL
 */
void display_signal(int sig, long long trade_no, long long ts_us,
                    double price, long long latency_ns, double pnl_usdt) {
    char clock[16];
    display_clock(ts_us, clock);
    printf(ANSI_CLEAR_LINE "%s%s%-4s" ANSI_RESET
           "  %s  trade %-9lld  price %.2f  latency %lld ns  P&L %+.2f USDT\n",
           ANSI_BOLD, sig == SIG_BUY ? ANSI_GREEN : ANSI_RED,
           sig == SIG_BUY ? "BUY" : "SELL",
           clock, trade_no, price, latency_ns, pnl_usdt);
}

/* display_status
 * What:    the live status bar. "\r" returns to the start of the line so it
 *          is redrawn in place. The lag turns red once we are > 1 ms behind.
 */
void display_status(const char *engine, double speed, long long ts_us,
                    long long done, long long total, double lag_ms,
                    double max_lag_ms, double pnl_usdt, int wins, int orders,
                    int buys, int sells) {
    char clock[16];
    display_clock(ts_us, clock);
    printf(ANSI_CLEAR_LINE ANSI_BOLD "[%-4s %6.0fx]" ANSI_RESET
           " %s  %5.1f%%  lag %s%9.3f ms" ANSI_RESET " (max %.3f)"
           "  P&L %s%+10.2f" ANSI_RESET "  won %d/%d  B/S %d/%d",
           engine, speed, clock, 100.0 * (double)done / (double)total,
           lag_ms > 1.0 ? ANSI_RED : ANSI_GREEN, lag_ms, max_lag_ms,
           pnl_usdt >= 0 ? ANSI_GREEN : ANSI_RED, pnl_usdt,
           wins, orders, buys, sells);
    fflush(stdout);
}
