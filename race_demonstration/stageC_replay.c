/*
 * stageC_replay.c - Stage C real-time replay (Lucas)
 *
 * What it does:
 *   1) LOAD:    load_trades_fx() (engine/trades_fx_fread.c) reads the whole
 *               CSV with one fread into a malloc'd TradeFx array (prices in cents).
 *               Every trade's timestamp in that array is the replay clock.
 *   2) REPLAY:  trade i is released at   start + (time of trade i - time of
 *               the first trade) / speed.  The program waits for that moment,
 *               then gives the trade to the chosen engine and times it:
 *                 v0  = Version 0 (engine/stream_engine.c): reads the trade's
 *                       line with next_trade(), double prices, naive SMA
 *                 mem = fastest version (engine/fixed_engine.c, ladder V7): takes
 *                       the trade from the TradeFx array, fixed-point modules
 *   3) SIGNAL:  each BUY / SELL is printed in colour and, on the Pi, shown on
 *               an LED (green = BUY, red = SELL): --led = the onboard status
 *               LED, --led-gpio = breadboard LEDs on GPIO 17 (red) / 27 (green).
 *               With --led-every N the LED instead flashes green on every Nth
 *               BUY and red on every Nth SELL.
 *   4) SUMMARY: average and worst processing time and latency, the biggest lag
 *               behind the replay clock, and the P&L against the rival bot
 *
 * Usage (from the project root):
 *   stageC_replay <file.csv> [--engine v0|mem] [--speed X] [--minutes M]
 *                 [--from-min M] [--short S] [--long L] [--rival R_us]
 *                 [--led | --led-gpio] [--led-every N] [--signals | --quiet] [--feed N]
 *     --feed N        machine-readable live output for the GUI (stageC_gui) and
 *                     the GIF maker (plot_live.py): N "#F" sample lines spread
 *                     evenly over the replayed market time, one "#S" line per
 *                     signal, then the normal summary. Turns off the status and
 *                     signal lines. Printed after the time is taken, like them.
 *     --speed X      1 = real time, 100, 1000 ... ; 0 = no clock (as fast as possible)
 *     --minutes M     stop after M minutes of MARKET time (0 = the whole file)
 *     --from-min M    start M market minutes after the first trade
 *     --short/--long  SMA windows (default 35 and 60)
 *     --rival R_us    rival reaction time in microseconds (default 1)
 *     --led           drive the Pi's onboard status LED (run with sudo)
 *     --led-gpio      drive breadboard LEDs: GPIO 17 red, GPIO 27 green (run with sudo)
 *     --led-every N   flash the LED (200 ms) on every Nth BUY (green) and every Nth
 *                     SELL (red), counted separately; 0 = default: the LED holds
 *                     the colour of the latest signal
 *     --signals       print every signal
 *     --quiet         print only the summary (no signal or status lines)
 *     --percentiles   also keep every processing time and latency in two malloc'd
 *                     arrays (16 bytes per trade), sort them after the replay with
 *                     quick_sort (engine/percentile.c) and print p50 and p99.
 *                     Off by default, so the default timings stay comparable.
 *
 * Examples (from the project root):
 *   test20 check:   ./race_demonstration/stageC_replay data/group_data/test20.csv --short 3 --long 5 --speed 0 --signals --engine v0
 *   10 minutes at real speed with the LED (Pi):
 *                   sudo taskset -c 2 ./race_demonstration/stageC_replay data/group_data/stress_full.csv --speed 1 --minutes 10 --led
 *   race (two terminals):
 *                   ./race_demonstration/stageC_replay data/group_data/stress_full.csv --engine v0  --speed 5000
 *                   sudo ./race_demonstration/stageC_replay data/group_data/stress_full.csv --engine mem --speed 5000 --led
 *
 * If you stop it early with Ctrl-C, the LED keeps its last colour: reboot the
 * Pi to give the onboard LED back to the system (see the brief, section 7).
 * The breadboard LEDs are switched off by the next --led-gpio run.
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* atoi, free */
#include <string.h>                      /* strcmp */
#include "../helper_function/led.h"
#include "engine/led_gpio.h"             /* breadboard LEDs, GPIO 17 / 27     */
#include "engine/signal.h"               /* SIG_BUY, SIG_SELL, SIG_NONE */
#include "engine/sma.h"                  /* MAX_WIN, the largest SMA window */
#include "engine/fixed.h"                /* PRICE_SCALE (cents)              */
#include "engine/trades_fx.h"            /* TradeFx, load_trades_fx() (fread loader) */
#include "engine/stream_engine.h"        /* Version 0 (ladder V0) */
#include "engine/fixed_engine.h"         /* fastest version (ladder V7) */
#include "engine/rival.h"                /* the rival bot                     */
#include "engine/percentile.h"           /* quick_sort, percentile (p50 / p99) */

/* ---- the two engines ---- */
#define ENGINE_V0  0
#define ENGINE_MEM 1

/* ---- which LED (opt_led) ---- */
#define LED_NONE    0
#define LED_ONBOARD 1                   /* --led:      Pi status LED (led.h)      */
#define LED_GPIO    2                   /* --led-gpio: GPIO 17 red, GPIO 27 green */

/* ---- terminal colours (ANSI escape codes) ---- */
#define GREEN "\033[32m"
#define RED   "\033[31m"
#define RESET "\033[0m"

/* ---- time units ---- */
#define NS_PER_MS        1000000LL      /* nanoseconds in a millisecond */
#define US_PER_MIN       60000000LL     /* microseconds in a minute     */
#define STATUS_EVERY_NS  1000000000LL   /* one status line per second   */
#define SLEEP_IF_MORE_NS 20000000LL     /* sleep if more than 20 ms early */
#define FLASH_NS         200000000LL    /* --led-every: one flash = 200 ms */

/* ---- options from the command line (filled in by read_options) ---- */
char  *opt_path = NULL;          /* the CSV file                        */
int    opt_engine = ENGINE_MEM;  /* ENGINE_V0 or ENGINE_MEM             */
int    opt_speed = 1000;         /* 0 = no clock                        */
int    opt_minutes = 0;          /* 0 = the whole file                  */
int    opt_from_min = 0;         /* minutes to skip at the start        */
int    opt_short = 35;           /* short SMA window                    */
int    opt_long = 60;            /* long SMA window                     */
double opt_rival_us = 1.0;       /* rival reaction time, microseconds   */
int    opt_led = LED_NONE;       /* LED_NONE, LED_ONBOARD or LED_GPIO   */
int    opt_led_every = 0;        /* > 0 = flash on every Nth BUY / SELL */
int    opt_signals = -1;         /* 1 = print signals, -1 = decide later */
int    opt_quiet = 0;            /* 1 = summary only                    */
int    opt_feed = 0;             /* > 0 = live feed with this many samples */
int    opt_pct = 0;              /* 1 = keep every time, report p50 / p99 */

/* colour the LED is showing now (SIG_NONE = off) */
int led_colour = SIG_NONE;

/* --led-every: BUY and SELL signals counted so far, and when the flash ends */
long long led_buys_seen = 0;
long long led_sells_seen = 0;
long long led_off_at = 0;

/* print_usage - how to run the program */
void print_usage(char *program) {
    printf("usage: %s <file.csv> [--engine v0|mem] [--speed X] [--minutes M]\n", program);
    printf("          [--from-min M] [--short S] [--long L] [--rival R_us]\n");
    printf("          [--led | --led-gpio] [--led-every N] [--signals | --quiet] [--feed N]\n");
    printf("          [--percentiles]\n");
}

/* read_options
 * What:    copy the command-line arguments into the opt_ variables
 * Steps:   look at argv[1], argv[2], ... one at a time. --led, --led-gpio,
 *          --signals and --quiet stand alone; every other option is followed
 *          by a value.
 * Returns: 1 if OK, 0 if an argument is wrong or missing
 */
int read_options(int argc, char *argv[]) {
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "--led") == 0) {
            opt_led = LED_ONBOARD;
        } else if (strcmp(argv[i], "--led-gpio") == 0) {
            opt_led = LED_GPIO;
        } else if (strcmp(argv[i], "--signals") == 0) {
            opt_signals = 1;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            opt_signals = 0;
            opt_quiet = 1;
        } else if (strcmp(argv[i], "--percentiles") == 0) {
            opt_pct = 1;
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
            } else if (strcmp(argv[i], "--feed") == 0) {
                opt_feed = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--led-every") == 0) {
                opt_led_every = atoi(argv[i + 1]);
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
    if (opt_speed < 0 || opt_minutes < 0 || opt_from_min < 0 || opt_feed < 0 ||
        opt_led_every < 0) {
        printf("speed, minutes, from-min, feed and led-every must not be negative\n");
        return 0;
    }
    /* the feed replaces the status and signal lines */
    if (opt_feed > 0) {
        opt_quiet = 1;
        opt_signals = 0;
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

/* feed_sample
 * What:    one "#F" line of the live feed (read by stageC_gui and plot_live.py):
 *          #F done total time_us price_cents pnl_lat pnl_proc lag_ns max_lag_ns
 *             buys sells won lost avg_proc_ns
 *          P&L values are in cents (reaction = latency, reaction = processing).
 * Note:    fflush, so a GUI reading through a pipe (or over ssh) sees it now
 */
void feed_sample(long long done, long long total, long long time_us, long long price,
                 Rival *rival, Rival *rival_proc, long long lag_ns, long long max_lag_ns,
                 int buys, int sells, double proc_total) {
    double avg_proc = 0.0;

    if (done > 0) {
        avg_proc = proc_total / done;
    }
    printf("#F %lld %lld %lld %lld %lld %lld %lld %lld %d %d %d %d %.1f\n", done, total,
           time_us, price, rival->pnl, rival_proc->pnl, lag_ns, max_lag_ns, buys, sells,
           rival->wins, rival->losses, avg_proc);
    fflush(stdout);
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

/* led_light
 * What:    switch the BUY colour (green) or the SELL colour (red) on (1) or
 *          off (0), on the onboard LED (--led) or the breadboard LEDs (--led-gpio)
 */
void led_light(int sig, int on) {
    int gpio_led = GPIO_LED_RED;
    int onboard_led = LED_RED;

    if (sig == SIG_BUY) {
        gpio_led = GPIO_LED_GREEN;
        onboard_led = LED_GREEN;
    }
    if (opt_led == LED_GPIO) {
        if (on) {
            gpio_led_on(gpio_led);
        } else {
            gpio_led_off(gpio_led);
        }
    } else {
        if (on) {
            led_on(onboard_led);
        } else {
            led_off(onboard_led);
        }
    }
}

/* show_led
 * What:    a signal on the LED.
 *          Default: green on for BUY, red on for SELL (the other one off),
 *          held until the next signal of the other kind.
 *          --led-every N: count BUYs and SELLs separately; on every Nth one,
 *          flash its colour for FLASH_NS (end_flash switches it off).
 * Note:    called AFTER the time is measured: changing the LED is a system
 *          call that takes microseconds
 */
void show_led(int sig, long long now) {
    if (opt_led_every > 0) {
        if (sig == SIG_BUY) {
            led_buys_seen++;
            if (led_buys_seen % opt_led_every != 0) {
                return;
            }
        } else {
            led_sells_seen++;
            if (led_sells_seen % opt_led_every != 0) {
                return;
            }
        }
        if (led_colour != SIG_NONE && led_colour != sig) {
            led_light(led_colour, 0);   /* the other colour's flash ends early */
        }
        led_light(sig, 1);
        led_colour = sig;
        led_off_at = now + FLASH_NS;
        return;
    }
    if (sig == led_colour) {
        return;                         /* already showing this colour */
    }
    if (led_colour != SIG_NONE) {
        led_light(led_colour, 0);
    }
    led_light(sig, 1);
    led_colour = sig;
}

/* end_flash
 * What:    --led-every: switch the LED off once its flash has lasted FLASH_NS
 * Note:    checked after every trade, after the time is taken
 */
void end_flash(long long now) {
    if (opt_led_every > 0 && led_colour != SIG_NONE && now >= led_off_at) {
        led_light(led_colour, 0);
        led_colour = SIG_NONE;
    }
}

int main(int argc, char *argv[]) {
    TradeFx *list;                     /* malloc'd array from load_trades_fx() */
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
    long long feed_step = 0, next_feed = 0;   /* market time between "#F" lines, us */
    int sig, buys = 0, sells = 0;
    double vwap = 0.0;
    char *name;
    Rival rival;                       /* reaction = latency (release -> decision) */
    Rival rival_proc;                  /* reaction = processing time (the brief)   */
    long long *proc_list = NULL;       /* --percentiles: every processing time, ns */
    long long *lat_list = NULL;        /* --percentiles: every latency, ns         */
    long long sort_start, sort_ns;     /* how long the two sorts took              */

    if (!read_options(argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }

    /* ---- 1) LOAD: the whole file into the TradeFx array (not part of the replay) ---- */
    load_start = now_ns();
    list = load_trades_fx(opt_path, &n);
    load_end = now_ns();
    if (list == NULL) {
        return 1;                      /* the loader already printed why */
    }

    /* ---- 2) pick the trades to replay: first .. last-1 ---- */
    ts_first = list[0].time + opt_from_min * US_PER_MIN;
    first = 0;
    while (first < n && list[first].time < ts_first) {
        first++;
    }
    if (first >= n) {
        printf("--from-min is past the end of the file\n");
        free_trades_fx(list);
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
        if (!stream_start(opt_path, opt_short, opt_long)) {
            free_trades_fx(list);
            return 1;
        }
        stream_skip(first);                /* move the file to trade "first" */
    } else {
        name = "mem";
        fixed_start(opt_short, opt_long);
    }
    rival_init(&rival, opt_rival_us);
    rival_init(&rival_proc, opt_rival_us);

    printf("Stage C replay  engine=%s  speed=%dx  windows=%d/%d  rival R=%g us\n",
           name, opt_speed, opt_short, opt_long, opt_rival_us);
    printf("file %s: %lld trades, replaying %lld (trades %lld..%lld), %.1f market minutes\n",
           opt_path, n, last - first, first + 1, last,
           (list[last - 1].time - ts_first) / 60e6);
    printf("load %.1f ms (load_trades_fx), array %lld x %d B = %.1f MB\n\n",
           (load_end - load_start) / 1e6, n, (int)sizeof(TradeFx),
           (double)n * sizeof(TradeFx) / 1e6);

    if (opt_led == LED_ONBOARD) {
        led_init();
    }
    if (opt_led == LED_GPIO) {
        gpio_led_init();
    }

    /* live feed: "#H engine speed short long rival_us trades ts_first_us span_us",
     * then a "#F" line every feed_step microseconds of market time */
    if (opt_feed > 0) {
        feed_step = (list[last - 1].time - ts_first) / opt_feed;
        if (feed_step < 1) {
            feed_step = 1;
        }
        next_feed = ts_first;
        printf("#H %s %d %d %d %g %lld %lld %lld\n", name, opt_speed, opt_short, opt_long,
               opt_rival_us, last - first, ts_first, list[last - 1].time - ts_first);
        fflush(stdout);
    }

    /* --percentiles: room for one processing time and one latency per trade */
    if (opt_pct) {
        proc_list = malloc((last - first) * sizeof(long long));
        lat_list = malloc((last - first) * sizeof(long long));
        if (proc_list == NULL || lat_list == NULL) {
            printf("not enough memory for --percentiles\n");
            free(proc_list);
            free(lat_list);
            free_trades_fx(list);
            return 1;
        }
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
            sig = stream_step();
        } else {
            sig = fixed_step(&list[i]);
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
        if (opt_pct) {
            proc_list[done] = proc_ns;
            lat_list[done] = lat_ns;
        }

        /* d) the rival bot: first fill an order lost on the previous signal
         *    at this trade's price, then react to this trade's signal */
        if (opt_engine == ENGINE_V0) {
            price = stream_price_cents();
            buys = stream_buys();
            sells = stream_sells();
        } else {
            price = fixed_price_cents(&list[i]);
            buys = fixed_buys();
            sells = fixed_sells();
        }
        rival_on_trade(&rival, price);
        rival_on_trade(&rival_proc, price);
        if (sig != SIG_NONE) {
            rival_on_signal(&rival, sig, lat_ns, price);
            rival_on_signal(&rival_proc, sig, proc_ns, price);
            if (opt_led) {
                show_led(sig, t_done);
            }
            if (opt_signals) {
                show_signal(sig, i + 1, list[i].time, price, lat_ns, rival.pnl);
            }
            if (opt_feed > 0) {
                /* #S sig time_us price_cents latency_ns */
                printf("#S %d %lld %lld %lld\n", sig, list[i].time, price, lat_ns);
            }
        }
        done++;

        /* live feed sample (after the time is taken, like the status line) */
        if (opt_feed > 0 && (list[i].time >= next_feed || i == last - 1)) {
            feed_sample(done, last - first, list[i].time, price, &rival, &rival_proc,
                        lag_ns, max_lag_ns, buys, sells, proc_total);
            while (next_feed <= list[i].time) {
                next_feed = next_feed + feed_step;
            }
        }

        /* e) a status line once a second (printing is not counted as latency) */
        now = now_ns();
        if (opt_led) {
            end_flash(now);
        }
        if (!opt_quiet && now >= next_status) {
            show_status(name, list[i].time, done, last - first, lag_ns, max_lag_ns,
                        &rival, buys, sells);
            next_status = now + STATUS_EVERY_NS;
        }
    }
    wall_end = now_ns();

    if (opt_led == LED_ONBOARD) {
        led_off(LED_GREEN);
        led_off(LED_RED);
        led_cleanup();
    }
    if (opt_led == LED_GPIO) {
        gpio_led_cleanup();            /* both off, pins released */
    }

    /* ---- 5) results from the engine ---- */
    if (opt_engine == ENGINE_V0) {
        buys = stream_buys();
        sells = stream_sells();
        vwap = stream_vwap();
        stream_stop();
    } else {
        buys = fixed_buys();
        sells = fixed_sells();
        vwap = fixed_vwap();
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

    /* --percentiles: sort both lists (after the replay, so not timed as latency) */
    if (opt_pct) {
        sort_start = now_ns();
        quick_sort(proc_list, 0, done - 1);
        quick_sort(lat_list, 0, done - 1);
        sort_ns = now_ns() - sort_start;
        printf("percentiles       processing p50 %lld ns   p99 %lld ns\n",
               percentile(proc_list, done, 50), percentile(proc_list, done, 99));
        printf("                  latency    p50 %lld ns   p99 %lld ns\n",
               percentile(lat_list, done, 50), percentile(lat_list, done, 99));
        printf("                  (2 x %lld values sorted in %.1f ms, middle-pivot quicksort)\n",
               done, sort_ns / 1e6);
        free(proc_list);
        free(lat_list);
    }
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

    free_trades_fx(list);
    return 0;
}
