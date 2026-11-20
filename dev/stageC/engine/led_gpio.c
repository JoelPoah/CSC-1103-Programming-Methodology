/*
 * led_gpio.c - Stage C: breadboard LEDs on GPIO 17 (red) and GPIO 27 (green)
 *
 * How it works (Linux sysfs GPIO, the same idea as led.h's /sys/class/leds):
 *   1) find the header GPIO chip: the folder /sys/class/gpio/gpiochipN whose
 *      "label" file says "pinctrl-rp1" (Pi 5) or "pinctrl-bcm..." (older Pis).
 *      N is that chip's first number, so GPIO 17 is number N + 17.
 *      (On a Pi 5 N is not 0, e.g. 571, so GPIO 17 is 588.)
 *   2) write that number to /sys/class/gpio/export, which creates the folder
 *      /sys/class/gpio/gpio<number>/, then write "out" to its "direction" file
 *   3) on / off = write "1" / "0" to its "value" file
 *   4) at the end: both off, back to "in", write the number to "unexport"
 *
 * Writing these files needs sudo. If a step fails (not a Pi, no sudo), the LED
 * state is printed instead, e.g. "[GPIO 27 GREEN ON]", like led.h does.
 */
#include <stdio.h>
#include <string.h>
#include "led_gpio.h"

#define GPIO_DIR      "/sys/class/gpio"
#define GPIO_MAX_CHIP 2048          /* gpiochip numbers to look at */

int gpio_hw = 0;                    /* 1 = the real pins are in use        */
int gpio_number[2] = { -1, -1 };    /* sysfs number of the green / red pin */
int gpio_pin[2] = { GPIO_PIN_GREEN, GPIO_PIN_RED };
char *gpio_name[2] = { "GREEN", "RED" };

/* gpio_write
 * What:    write a short text value to a sysfs file
 * Returns: 1 if it worked, 0 if not
 */
int gpio_write(char *path, char *value) {
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        return 0;
    }
    fputs(value, fp);
    if (fclose(fp) != 0) {
        return 0;
    }
    return 1;
}

/* gpio_find_base
 * What:    the first number of the GPIO chip whose label starts with prefix
 * Returns: that number, or -1 if there is no such chip
 */
int gpio_find_base(char *prefix) {
    char path[96];
    char label[64];
    FILE *fp;
    int n;

    for (n = 0; n < GPIO_MAX_CHIP; n++) {
        snprintf(path, sizeof path, GPIO_DIR "/gpiochip%d/label", n);
        fp = fopen(path, "r");
        if (fp != NULL) {
            label[0] = '\0';
            if (fgets(label, sizeof label, fp) == NULL) {
                label[0] = '\0';
            }
            fclose(fp);
            if (strncmp(label, prefix, strlen(prefix)) == 0) {
                return n;
            }
        }
    }
    return -1;
}

/* gpio_setup_pin
 * What:    export one pin and make it an output that is off
 * Returns: 1 if it worked, 0 if not
 */
int gpio_setup_pin(int number) {
    char path[96];
    char text[16];

    snprintf(text, sizeof text, "%d", number);
    gpio_write(GPIO_DIR "/export", text);       /* fails if already exported: fine */
    snprintf(path, sizeof path, GPIO_DIR "/gpio%d/direction", number);
    if (!gpio_write(path, "out")) {
        return 0;
    }
    snprintf(path, sizeof path, GPIO_DIR "/gpio%d/value", number);
    return gpio_write(path, "0");
}

/* gpio_release_pin - off, back to an input, unexport */
void gpio_release_pin(int number) {
    char path[96];
    char text[16];

    snprintf(path, sizeof path, GPIO_DIR "/gpio%d/value", number);
    gpio_write(path, "0");
    snprintf(path, sizeof path, GPIO_DIR "/gpio%d/direction", number);
    gpio_write(path, "in");
    snprintf(text, sizeof text, "%d", number);
    gpio_write(GPIO_DIR "/unexport", text);
}

void gpio_led_init(void) {
    int base = gpio_find_base("pinctrl-rp1");    /* Pi 5 */

    if (base < 0) {
        base = gpio_find_base("pinctrl-bcm");    /* Pi 4 and older */
    }
    if (base < 0) {
        printf("[led_gpio] No Pi GPIO pins found (%s). Printing the LED state instead.\n",
               GPIO_DIR);
        return;
    }
    gpio_number[GPIO_LED_GREEN] = base + GPIO_PIN_GREEN;
    gpio_number[GPIO_LED_RED] = base + GPIO_PIN_RED;
    if (gpio_setup_pin(gpio_number[GPIO_LED_GREEN]) &&
        gpio_setup_pin(gpio_number[GPIO_LED_RED])) {
        gpio_hw = 1;
        printf("[led_gpio] breadboard LEDs: GPIO %d red (sysfs %d), GPIO %d green (sysfs %d)\n",
               GPIO_PIN_RED, gpio_number[GPIO_LED_RED],
               GPIO_PIN_GREEN, gpio_number[GPIO_LED_GREEN]);
        return;
    }
    printf("[led_gpio] Cannot use GPIO %d / %d (run with sudo). Printing the LED state instead.\n",
           GPIO_PIN_RED, GPIO_PIN_GREEN);
}

/* gpio_led_set - one LED on (1) or off (0) */
void gpio_led_set(int led, int on) {
    char path[96];

    if (gpio_hw) {
        snprintf(path, sizeof path, GPIO_DIR "/gpio%d/value", gpio_number[led]);
        if (on) {
            gpio_write(path, "1");
        } else {
            gpio_write(path, "0");
        }
        return;
    }
    if (on) {
        printf("[GPIO %d %s ON]\n", gpio_pin[led], gpio_name[led]);
    } else {
        printf("[GPIO %d %s OFF]\n", gpio_pin[led], gpio_name[led]);
    }
}

void gpio_led_on(int led) {
    gpio_led_set(led, 1);
}

void gpio_led_off(int led) {
    gpio_led_set(led, 0);
}

void gpio_led_cleanup(void) {
    if (gpio_hw) {
        gpio_release_pin(gpio_number[GPIO_LED_GREEN]);
        gpio_release_pin(gpio_number[GPIO_LED_RED]);
        gpio_hw = 0;
    }
}
