/*
 * stageC_replay.c - Stage C real-time replay (Lucas)
 *
 * What it does:
 *   1) LOAD:    Leroi's load_trades_malloc() reads the whole CSV into an array
 *               made with malloc to fit the file. Every trade's timestamp in
 *               that array is the replay clock.
 *   2) REPLAY:  trade i is released at   start + (time of trade i - time of
 *               the first trade) / speed.  The program waits for that moment,
 *               then gives the trade to the chosen engine and times it:
 *                 v0  = Version 0 (Stage A): reads the trade's line from the
 *                       file with next_trade() and uses the Stage A modules
 *                 mem = fastest version: takes the trade from Leroi's array
 *                       and uses Leroi's Stage B fixed-point modules
 *   3) SIGNAL:  each BUY / SELL is printed in colour and, with --led on the Pi,
 *               shown on the status LED (green = BUY, red = SELL)
 *   4) SUMMARY: average and worst processing time and latency, the biggest lag
 *               behind the replay clock, and the P&L against the rival bot
 *
 * Usage (from the project root):
 *   stageC_replay <file.csv> [--engine v0|mem] [--speed X] [--minutes M]
 *                 [--from-min M] [--short S] [--long L] [--rival R_us]
 *                 [--led] [--signals | --quiet]
 *     --speed X       1 = real time, 100, 1000 ... ; 0 = no clock (as fast as possible)
 *     --minutes M     stop after M minutes of MARKET time (0 = the whole file)
 *     --from-min M    start M market minutes after the first trade
 *     --short/--long  SMA windows (default 35 and 60)
 *     --rival R_us    rival reaction time in microseconds (default 1)
 *     --led           drive the Pi's status LED (run with sudo)
 *     --signals       print every signal
 *     --quiet         print only the summary (no signal or status lines)
 *
 * Examples (from the project root):
 *   test20 check:   ./stageC/stageC_replay data/test20.csv --short 3 --long 5 --speed 0 --signals --engine v0
 *   10 minutes at real speed with the LED (Pi):
 *                   sudo taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --speed 1 --minutes 10 --led
 *   race (two terminals):
 *                   ./stageC/stageC_replay data/stress_full.csv --engine v0  --speed 5000
 *                   sudo ./stageC/stageC_replay data/stress_full.csv --engine mem --speed 5000 --led
 *
 * If you stop it early with Ctrl-C, the LED keeps its last colour: reboot the
 * Pi to give the LED back to the system (see the brief, section 7).
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* atoi, free */
#include <string.h>                      /* strcmp */
#include "../helper_function/led.h"
#include "../stageB/engine/signal.h"     /* SIG_BUY, SIG_SELL, SIG_NONE */
#include "../stageB/engine/sma.h"        /* MAX_WIN, the largest SMA window */
#include "../stageB/engine/fixed.h"      /* PRICE_SCALE (cents)              */
#include "engine/trades_malloc.h"        /* Leroi: Trade, load_trades_malloc() */
#include "engine/v0_engine.h"            /* Version 0 (Stage A code)          */
#include "engine/mem_engine.h"           /* fastest version (Stage B code)    */
#include "engine/rival.h"                /* the rival bot                     */

/* ---- the two engines ---- */
#define ENGINE_V0  0
#define ENGINE_MEM 1

/* ---- terminal colours (ANSI escape codes) ---- */
#define GREEN "\033[32m"
#define RED   "\033[31m"
#define RESET "\033[0m"

/* ---- time units ---- */
#define NS_PER_MS        1000000LL      /* nanoseconds in a millisecond */
#define US_PER_MIN       60000000LL     /* microseconds in a minute     */
#define STATUS_EVERY_NS  1000000000LL   /* one status line per second   */
#define SLEEP_IF_MORE_NS 20000000LL     /* sleep if more than 20 ms early */

/* ---- options from the command line (filled in by read_options) ---- */
char  *opt_path = NULL;          /* the CSV file                        */
int    opt_engine = ENGINE_MEM;  /* ENGINE_V0 or ENGINE_MEM             */
int    opt_speed = 1000;         /* 0 = no clock                        */
int    opt_minutes = 0;          /* 0 = the whole file                  */
int    opt_from_min = 0;         /* minutes to skip at the start        */
int    opt_short = 35;           /* short SMA window                    */
int    opt_long = 60;            /* long SMA window                     */
double opt_rival_us = 1.0;       /* rival reaction time, microseconds   */
int    opt_led = 0;              /* 1 = drive the LED                   */
int    opt_signals = -1;         /* 1 = print signals, -1 = decide later */
int    opt_quiet = 0;            /* 1 = summary only                    */

/* colour the LED is showing now (SIG_NONE = off) */
int led_colour = SIG_NONE;

/* print_usage - how to run the program */
void print_usage(char *program) {
    printf("usage: %s <file.csv> [--engine v0|mem] [--speed X] [--minutes M]\n", program);
    printf("          [--from-min M] [--short S] [--long L] [--rival R_us]\n");
    printf("          [--led] [--signals | --quiet]\n");
}

/* read_options
 * What:    copy the command-line arguments into the opt_ variables
 * Steps:   look at argv[1], argv[2], ... one at a time. --led, --signals and
 *          --quiet stand alone; every other option is followed by a value.
 * Returns: 1 if OK, 0 if an argument is wrong or missing
 */
int read_options(int argc, char *argv[]) {
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "--led") == 0) {
            opt_led = 1;
        } else if (strcmp(argv[i], "--signals") == 0) {
            opt_signals = 1;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            opt_signals = 0;
            opt_quiet = 1;
        } else if (argv[i][0] != '-' && opt_path == NULL) {
            opt_path = argv[i];
        } else if (i + 1 < argc) {
            /* an option with a value: the value is argv[i + 1] */
            if (strcmp(argv[i], "--engine") == 0) {
                if (strcmp(argv[i + 1], "v0") == 0) {
                    opt_engine = ENGINE_V0;
                } else if (strcmp(argv[i + 1], "mem") == 0) {
                    opt_engine = ENGINE_MEM;
                } else {
                    printf("unknown engine '%s' (use v0 or mem)\n", argv[i + 1]);
                    return 0;
                }
            } else if (strcmp(argv[i], "--speed") == 0) {
                opt_speed = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--minutes") == 0) {
                opt_minutes = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--from-min") == 0) {
                opt_from_min = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--short") == 0) {
                opt_short = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--long") == 0) {
                opt_long = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--rival") == 0) {
                if (sscanf(argv[i + 1], "%lf", &opt_rival_us) != 1) {
                    printf("bad rival time: %s\n", argv[i + 1]);
                    return 0;
                }
            } else {
                printf("bad argument: %s\n", argv[i]);
                return 0;
            }
            i++;                               /* skip over the value */
        } else {
            printf("bad or incomplete argument: %s\n", argv[i]);
            return 0;
        }
        i++;
    }

    if (opt_path == NULL) {
        return 0;
    }
    if (opt_short < 1 || opt_short >= opt_long || opt_long > MAX_WIN) {
        printf("bad windows: need 1 <= short < long <= %d\n", MAX_WIN);
        return 0;
    }
    if (opt_speed < 0 || opt_minutes < 0 || opt_from_min < 0) {
        printf("speed, minutes and from-min must not be negative\n");
        return 0;
    }
    /* by default, print each signal only at real speed up to 100x
     * (at higher speeds there are too many to read) */
    if (opt_signals == -1) {
        if (opt_speed > 0 && opt_speed <= 100) {
            opt_signals = 1;
        } else {
            opt_signals = 0;
        }
    }
    return 1;
}

/* print_clock
 * What:    print a trade's time of day as HH:MM:SS (UTC)
 * Steps:   microseconds -> seconds -> seconds into the day -> hours, minutes, seconds
 */
void print_clock(long long time_us) {
    long long day_sec = (time_us / 1000000LL) % 86400LL;

    printf("%02lld:%02lld:%02lld", day_sec / 3600, (day_sec / 60) % 60, day_sec % 60);
}

/* show_signal
 * What:    one line per signal: BUY in green, SELL in red
 */
void show_signal(int sig, long long trade_no, long long time_us, long long price_cents,
                 long long latency_ns, long long pnl_cents) {
    if (sig == SIG_BUY) {
        printf(GREEN "BUY " RESET);
    } else {
        printf(RED "SELL" RESET);
    }
    printf("  ");
    print_clock(time_us);
    printf("  trade %-9lld  price %.2f  latency %lld ns  P&L %+.2f USDT\n", trade_no,
           (double)price_cents / PRICE_SCALE, latency_ns, (double)pnl_cents / PRICE_SCALE);
}

/* show_status
 * What:    one status line: market time, progress, lag (red once more than
 *          1 ms behind the replay clock), P&L and the signal counts
 */
void show_status(char *name, long long time_us, long long done, long long total,
                 long long lag_ns, long long max_lag_ns, Rival *rival, int buys, int sells) {
    printf("[%-3s %dx] ", name, opt_speed);
    print_clock(time_us);
    printf("  %5.1f%%  lag ", 100.0 * done / total);
    if (lag_ns > NS_PER_MS) {
        printf(RED);
    } else {
        printf(GREEN);
    }
    printf("%9.3f ms" RESET " (max %.3f)  P&L %+10.2f  won %d/%d  B/S %d/%d\n",
           (double)lag_ns / NS_PER_MS, (double)max_lag_ns / NS_PER_MS,
           (double)rival->pnl / PRICE_SCALE, rival->wins, rival->wins + rival->losses,
           buys, sells);
}

/* wait_until
 * What:    do nothing until the clock reaches target (in ns)
 * Why:     sleeping can overshoot by several milliseconds, far longer than
 *          one trade, so we only sleep when we are more than 20 ms early and
 *          keep checking the clock for the last part
 */
void wait_until(long long target) {
    long long now = now_ns();

    while (now < target) {
        if (target - now > SLEEP_IF_MORE_NS) {
            sleep_ms(10);
        }
        now = now_ns();
    }
}

/* show_led
 * What:    green on for BUY, red on for SELL (the other one off)
 * Note:    called AFTER the time is measured: changing the LED is a system
 *          call that takes microseconds
 */
void show_led(int sig) {
    if (sig == led_colour) {
        return;                         /* already showing this colour */
    }
    if (sig == SIG_BUY) {
        led_off(LED_RED);
        led_on(LED_GREEN);
    } else {
        led_off(LED_GREEN);
        led_on(LED_RED);
    }
    led_colour = sig;
}

int main(int argc, char *argv[]) {
    Trade *list;                       /* Leroi's malloc'd array of trades   */
    long long n;                       /* how many trades it holds           */
    long long first, last, i;          /* replay trades first .. last-1      */
    long long done = 0;                /* trades replayed so far             */
    long long ts_first, ts_end;        /* market time of the first / end     */
    long long load_start, load_end;    /* clock readings, ns                 */
    long long start, wall_end, next_status, now;
    long long release = 0, t_start, t_done;
    long long proc_ns, lat_ns, lag_ns = 0, max_lag_ns = 0;
    long long proc_worst = 0, lat_worst = 0;
    double proc_total = 0.0, lat_total = 0.0;
    long long price;
    int sig, buys = 0, sells = 0;
    double vwap = 0.0;
    char *name;
    Rival rival;                       /* reaction = latency (release -> decision) */
    Rival rival_proc;                  /* reaction = processing time (the brief)   */

    if (!read_options(argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }

    /* ---- 1) LOAD: the whole file into Leroi's array (not part of the replay) ---- */
    load_start = now_ns();
    list = load_trades_malloc(opt_path, &n);
    load_end = now_ns();
    if (list == NULL) {
        return 1;                      /* Leroi's loader already printed why */
    }

    /* ---- 2) pick the trades to replay: first .. last-1 ---- */
    ts_first = list[0].time + opt_from_min * US_PER_MIN;
    first = 0;
    while (first < n && list[first].time < ts_first) {
        first++;
    }
    if (first >= n) {
        printf("--from-min is past the end of the file\n");
        free(list);
        return 1;
    }
    ts_first = list[first].time;
    last = n;
    if (opt_minutes > 0) {
        ts_end = ts_first + opt_minutes * US_PER_MIN;
        last = first;
        while (last < n && list[last].time < ts_end) {
            last++;
        }
    }

    /* ---- 3) get the engine ready ---- */
    if (opt_engine == ENGINE_V0) {
        name = "v0";
        if (!v0_start(opt_path, opt_short, opt_long)) {
            free(list);
            return 1;
        }
        v0_skip(first);                /* move the file to trade "first" */
    } else {
        name = "mem";
        mem_start(opt_short, opt_long);
    }
    rival_init(&rival, opt_rival_us);
    rival_init(&rival_proc, opt_rival_us);

    printf("Stage C replay  engine=%s  speed=%dx  windows=%d/%d  rival R=%g us\n",
           name, opt_speed, opt_short, opt_long, opt_rival_us);
    printf("file %s: %lld trades, replaying %lld (trades %lld..%lld), %.1f market minutes\n",
           opt_path, n, last - first, first + 1, last,
           (list[last - 1].time - ts_first) / 60e6);
    printf("load %.1f ms (Leroi's load_trades_malloc), array %lld x %d B = %.1f MB\n\n",
           (load_end - load_start) / 1e6, n, (int)sizeof(Trade),
           (double)n * sizeof(Trade) / 1e6);

    if (opt_led) {
        led_init();
    }

    /* ---- 4) REPLAY ---- */
    start = now_ns();
    if (opt_speed > 0) {
        start = start + NS_PER_MS;     /* first trade is released 1 ms from now */
    }
    next_status = start + STATUS_EVERY_NS;

    for (i = first; i < last; i++) {
        /* a) when is trade i released? (speed 0: straight away) */
        if (opt_speed > 0) {
            release = start + (list[i].time - ts_first) * 1000 / opt_speed;
            wait_until(release);
        }

        /* b) process the trade, timing ONLY the engine */
        t_start = now_ns();
        if (opt_engine == ENGINE_V0) {
            sig = v0_step();
        } else {
            sig = mem_step(&list[i]);
        }
        t_done = now_ns();

        /* c) processing time, latency and lag of this trade */
        if (opt_speed == 0) {
            release = t_start;         /* no clock: latency = processing time */
        }
        proc_ns = t_done - t_start;
        lat_ns = t_done - release;
        lag_ns = 0;
        if (t_start > release) {
            lag_ns = t_start - release;
        }
        proc_total = proc_total + proc_ns;
        lat_total = lat_total + lat_ns;
        if (proc_ns > proc_worst) {
            proc_worst = proc_ns;
        }
        if (lat_ns > lat_worst) {
            lat_worst = lat_ns;
        }
        if (lag_ns > max_lag_ns) {
            max_lag_ns = lag_ns;
        }

        /* d) the rival bot: first fill an order lost on the previous signal
         *    at this trade's price, then react to this trade's signal */
        if (opt_engine == ENGINE_V0) {
            price = v0_price_cents();
            buys = v0_buys();
            sells = v0_sells();
        } else {
            price = mem_price_cents(&list[i]);
            buys = mem_buys();
            sells = mem_sells();
        }
        rival_on_trade(&rival, price);
        rival_on_trade(&rival_proc, price);
        if (sig != SIG_NONE) {
            rival_on_signal(&rival, sig, lat_ns, price);
            rival_on_signal(&rival_proc, sig, proc_ns, price);
            if (opt_led) {
                show_led(sig);
            }
            if (opt_signals) {
                show_signal(sig, i + 1, list[i].time, price, lat_ns, rival.pnl);
            }
        }
        done++;

        /* e) a status line once a second (printing is not counted as latency) */
        now = now_ns();
        if (!opt_quiet && now >= next_status) {
            show_status(name, list[i].time, done, last - first, lag_ns, max_lag_ns,
                        &rival, buys, sells);
            next_status = now + STATUS_EVERY_NS;
        }
    }
    wall_end = now_ns();

    if (opt_led) {
        led_off(LED_GREEN);
        led_off(LED_RED);
        led_cleanup();
    }

    /* ---- 5) results from the engine ---- */
    if (opt_engine == ENGINE_V0) {
        buys = v0_buys();
        sells = v0_sells();
        vwap = v0_vwap();
        v0_stop();
    } else {
        buys = mem_buys();
        sells = mem_sells();
        vwap = mem_vwap();
    }
    if (!opt_quiet) {
        show_status(name, list[first + done - 1].time, done, last - first, lag_ns,
                    max_lag_ns, &rival, buys, sells);
    }

    /* ---- 6) SUMMARY ---- */
    printf("\n==== summary (engine=%s, speed=%dx) ====\n", name, opt_speed);
    printf("trades replayed   %lld   market time %.1f s   wall time %.3f s\n", done,
           (list[first + done - 1].time - ts_first) / 1e6, (wall_end - start) / 1e9);
    printf("throughput        %.0f trades/s (wall)\n", done / ((wall_end - start) / 1e9));
    printf("signals           %d BUY, %d SELL   final VWAP %.8f\n", buys, sells, vwap);
    printf("processing time   avg %.1f ns   worst %lld ns\n", proc_total / done, proc_worst);
    printf("latency (release->decision)\n");
    printf("                  avg %.1f ns   worst %lld ns\n", lat_total / done, lat_worst);
    printf("max lag behind replay clock  %.3f ms\n", (double)max_lag_ns / NS_PER_MS);
    printf("rival bot (R=%g us)", opt_rival_us);
    if (rival.holding) {
        printf("   (1 BTC still held at the end, not counted)");
    }
    printf("\n");
    printf("  reaction = latency          P&L %+.2f USDT over %d round trips, won %d / lost %d\n",
           (double)rival.pnl / PRICE_SCALE, rival.round_trips, rival.wins, rival.losses);
    printf("  reaction = processing time  P&L %+.2f USDT over %d round trips, won %d / lost %d\n",
           (double)rival_proc.pnl / PRICE_SCALE, rival_proc.round_trips,
           rival_proc.wins, rival_proc.losses);

    free(list);                        /* give Leroi's array back */
    return 0;
}
