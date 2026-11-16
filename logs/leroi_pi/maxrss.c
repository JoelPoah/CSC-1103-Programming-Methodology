/*
 * maxrss.c - run a program and print its peak physical memory (max RSS)
 *
 * Same counter as "Maximum resident set size" in /usr/bin/time -v, but it
 * works on any Linux machine without installing GNU time.
 *
 * Why not ladder_bench's number: it measures from a forked copy of itself, so
 * the result can never be below the bench tool's own size (~15 MB). Here the
 * child is a copy of this tiny program until exec, so the floor is ~1 MB.
 *
 * Build:  gcc -O2 -Wall logs/leroi_pi/maxrss.c -o logs/leroi_pi/maxrss
 * Run:    ./logs/leroi_pi/maxrss ./bin/v6_stageC_malloc dev > /dev/null
 *         -> prints "maxrss_kB <number>" on stderr
 */
#include <stdio.h>
#include <unistd.h>         /* fork, execvp */
#include <sys/wait.h>       /* wait4 */
#include <sys/resource.h>   /* struct rusage */

int main(int argc, char *argv[]) {
    struct rusage ru;
    int status;
    pid_t pid;

    if (argc < 2) {
        fprintf(stderr, "usage: %s program [args]\n", argv[0]);
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {                     /* child: become the program */
        execvp(argv[1], &argv[1]);
        perror("execvp");               /* only reached if exec failed */
        _exit(127);
    }
    /* parent: wait for that one child and read its resource usage */
    if (wait4(pid, &status, 0, &ru) < 0) {
        perror("wait4");
        return 1;
    }
    fprintf(stderr, "maxrss_kB %ld\n", ru.ru_maxrss);   /* kB on Linux */
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
