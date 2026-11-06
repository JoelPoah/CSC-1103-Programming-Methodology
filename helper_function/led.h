/*
 * led.h  -  PM Mini Project helper (works on Windows, macOS and Raspberry Pi)
 *
 *   led_init()        call once at the start of main()
 *   led_on(LED_GREEN) led_off(LED_GREEN)
 *   led_on(LED_RED)   led_off(LED_RED)
 *   led_cleanup()     call once at the end of main()
 *
 * On a Raspberry Pi 5 (run with sudo) this drives the built-in status LED.
 * On Windows, macOS, or a Pi without sudo, it prints the LED state instead,
 * e.g. "[LED GREEN ON]", so the same program runs everywhere.
 */
#ifndef LED_H
#define LED_H

#include <stdio.h>

#define LED_GREEN 0
#define LED_RED   1

static const char *led_name[2] = { "GREEN", "RED" };

#if defined(__linux__)
static int led_hw = 0;                                   /* 1 = real LED in use */
static const char *led_dir[2] = { "/sys/class/leds/ACT", "/sys/class/leds/PWR" };

/* write a short text value to one of the LED's control files */
static inline int led_write(int led, const char *file, const char *value) {
    char path[128];
    FILE *fp;
    snprintf(path, sizeof path, "%s/%s", led_dir[led], file);
    fp = fopen(path, "w");
    if (fp == NULL) return 0;
    fputs(value, fp);
    fclose(fp);
    return 1;
}
#endif

static inline void led_init(void) {
#if defined(__linux__)
    led_hw = led_write(LED_GREEN, "trigger", "none") && led_write(LED_RED, "trigger", "none");
    if (led_hw) {
        led_write(LED_GREEN, "brightness", "0");
        led_write(LED_RED, "brightness", "0");
        return;
    }
    printf("[led.h] Built-in LED not available (not a Pi 5, or not run with sudo). Printing instead.\n");
#else
    printf("[led.h] No built-in LED on this computer. Printing instead.\n");
#endif
}

static inline void led_set(int led, int on) {
#if defined(__linux__)
    if (led_hw) { led_write(led, "brightness", on ? "1" : "0"); return; }
#endif
    printf("[LED %s %s]\n", led_name[led], on ? "ON" : "OFF");
}

static inline void led_on(int led)  { led_set(led, 1); }
static inline void led_off(int led) { led_set(led, 0); }

static inline void led_cleanup(void) {
#if defined(__linux__)
    if (led_hw) {                                /* give the LEDs back to the system */
        led_write(LED_GREEN, "trigger", "mmc0");
        led_write(LED_RED, "trigger", "default-on");
    }
#endif
}

#endif /* LED_H */
