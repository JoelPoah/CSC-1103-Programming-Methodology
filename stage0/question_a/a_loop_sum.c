#include <stdio.h>
#include <inttypes.h>
#include "../../helper_function/timing.h"

int counter(int n);

int main()
{
    /* question a: this program the value of n is entered by the user. */
    uint64_t start, end, n;
    uint64_t total = 0;
    long long input;
    char extra;

    /* keep asking until the user enters a positive whole number */
    while (1)
    {
        printf("Enter value of N: ");
        extra = 0;
        if (scanf("%lld%c", &input, &extra) == 2 && extra == '\n' && input > 0)
        {
            break;
        }
        
        printf("Invalid input. Please enter a positive whole number.\n");
        if (extra != '\n')
        {
            while (getchar() != '\n');
        }
    }
    n = (uint64_t)input;

    start = now_ns();

    for (uint64_t i = 1; i <= n; i++)
    {
        total += i;
    }

    end = now_ns();
    printf("sum: %" PRIu64 "\n", total);
    printf("Time taken: %" PRIu64 " ns\n", end - start);

}
