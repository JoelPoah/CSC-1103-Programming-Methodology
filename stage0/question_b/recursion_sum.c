#include <stdio.h>
#include <inttypes.h>
#include "../../helper_function/timing.h"
#include "../../helper_function/led.h"

int counter(int n);

int main()
{
    led_init();
    int n = 10000;
    uint64_t start, end, result;

    led_on(LED_GREEN); 
    start = now_ns();
    result = counter(n);
    end = now_ns();
    led_off(LED_GREEN);
    printf("sum: %" PRIu64 "\n", result);
    printf("Time taken: %" PRIu64 " ns\n", end - start);
    led_cleanup();
    return 0;
}

int counter(int n)
{
    if (n <= 0)
    {
        return 0;
    }

    return n + counter(n - 1);
}