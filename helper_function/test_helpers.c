/* test_helpers.c - checks that timing.h and led.h work on your computer.
 *
 * Windows / Mac:  gcc test_helpers.c -o test_helpers        then  ./test_helpers
 * Raspberry Pi:   gcc test_helpers.c -o test_helpers        then  sudo ./test_helpers
 */
#include "timing.h"
#include "led.h"
#include <stdio.h>

int main(void) {
    uint64_t start, end;
    int i;

    led_init();
    for (i = 0; i < 3; i++) {               /* flash green, then red, three times */
        led_on(LED_GREEN);  sleep_ms(300);  led_off(LED_GREEN);
        led_on(LED_RED);    sleep_ms(300);  led_off(LED_RED);
    }

    start = now_ns();
    sleep_ms(100);
    end = now_ns();
    printf("Asked to sleep 100 ms, measured %.3f ms\n", (end - start) / 1e6);

    led_cleanup();
    return 0;
}
