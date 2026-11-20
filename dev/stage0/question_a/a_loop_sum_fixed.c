#include <stdio.h>
#include <inttypes.h>
#include "../../../helper_function/timing.h"
#include "../../../helper_function/led.h"

int counter(int n);

int main()
{
    /* question a: This program the value of n is fixed within the program. */
    uint64_t start, end;
    uint64_t n = 100000000;
    uint64_t total = 0;

    led_init(); //  run in sudo on pi
    led_on(LED_GREEN);
    start = now_ns();
    for (uint64_t i = 1; i <= n; i++)
    {
        total += i;
    }
    end = now_ns();
    led_off(LED_GREEN);

    printf("sum: %" PRIu64 "\n", total);
    printf("Time taken: %" PRIu64 " ns\n", end - start);

    led_cleanup();
    return 0;
}
