/*
 * display.h - coloured terminal output for the replay (Stage C)
 *
 * Uses ANSI escape codes: \033[32m green (BUY), \033[31m red (SELL),
 * \033[0m back to normal. Windows 10+ terminals understand them once
 * "virtual terminal processing" is switched on (display_init does that).
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#define ANSI_GREEN  "\033[32m"
#define ANSI_RED    "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_RESET  "\033[0m"
#define ANSI_CLEAR_LINE "\033[2K\r"

/* Call once at the start (turns on ANSI colours on Windows). */
void display_init(void);

/* "HH:MM:SS" (UTC) for a timestamp in microseconds; out needs 16 chars. */
void display_clock(long long ts_us, char *out);

/* One coloured line for a signal. */
void display_signal(int sig, long long trade_no, long long ts_us,
                    double price, long long latency_ns, double pnl_usdt);

/* Overwrite the one-line status bar (no newline). */
void display_status(const char *engine, double speed, long long ts_us,
                    long long done, long long total, double lag_ms,
                    double max_lag_ms, double pnl_usdt, int wins, int orders,
                    int buys, int sells);

#endif /* DISPLAY_H */
