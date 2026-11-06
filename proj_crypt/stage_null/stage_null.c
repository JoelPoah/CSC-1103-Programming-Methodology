#include <stdio.h>
#include <inttypes.h>
#include "../../helper_function/timing.h"
#include "../../helper_function/led.h"

int counter(int n);

int main()
{
    // question a:
    /*
    uint64_t start, end;
    uint64_t n = 100000000;
    uint64_t total = 0;

    start = now_ns();

    for (uint64_t i = 1; i <= n; i++)
    {
        total += i;
    }

    end = now_ns();
    printf("sum: %" PRIu64 "\n", total);
    printf("Time taken: %" PRIu64 " ns\n", end - start);
    */

    // question b
    led_init();
    int n = 10000;
    uint64_t start, end;

    led_on(LED_GREEN); 
    start = now_ns();
    printf("sum: %d\n", counter(n));
    end = now_ns();
    led_off(LED_GREEN);

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