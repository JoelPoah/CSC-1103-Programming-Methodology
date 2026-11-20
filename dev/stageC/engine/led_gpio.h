/*
 * led_gpio.h - Stage C: red / green LEDs on a breadboard, on the Pi's GPIO pins
 *
 *   GPIO 17 = red LED  (SELL)       GPIO 27 = green LED (BUY)
 *   (BCM numbers: GPIO 17 is header pin 11, GPIO 27 is header pin 13)
 *
 *   gpio_led_init()                 call once before the replay
 *   gpio_led_on(GPIO_LED_GREEN)     gpio_led_off(GPIO_LED_GREEN)
 *   gpio_led_on(GPIO_LED_RED)       gpio_led_off(GPIO_LED_RED)
 *   gpio_led_cleanup()              call once at the end (both off, pins released)
 *
 * Used by stageC_replay --led-gpio. The onboard status LED (--led) still uses
 * the course's helper_function/led.h. Like led.h, if the pins cannot be used
 * (not a Pi, or not run with sudo) it prints the LED state instead.
 */
#ifndef LED_GPIO_H
#define LED_GPIO_H

#define GPIO_LED_GREEN 0
#define GPIO_LED_RED   1

#define GPIO_PIN_GREEN 27
#define GPIO_PIN_RED   17

void gpio_led_init(void);
void gpio_led_on(int led);
void gpio_led_off(int led);
void gpio_led_cleanup(void);

#endif
