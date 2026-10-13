#include <stdio.h>
#include <inttypes.h>
#include "../../helper_function/timing.h"
#include "../../helper_function/led.h"

int counter(int n);

int main()
{
    // question a:
    uint64_t start, end;
    uint64_t n = 10000;
    uint64_t total = 0;

    start = now_ns();

    for (uint64_t i = 1; i <= n; i++)
    {
        total += i;
    }

    end = now_ns();
    printf("sum: %" PRIu64 "\n", total);
    printf("Time taken: %" PRIu64 " ns\n", end - start);
}
