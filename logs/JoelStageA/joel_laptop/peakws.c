/*
 * peakws.c - run a program on Windows and print its peak working set
 *
 * The Windows version of logs/leroi_pi/maxrss.c. It reads the same counter that
 * benchmark/src/run.c uses for ladder_bench's "peak working set", but it can run
 * any dataset (ladder_bench only runs stress).
 *
 * Build:  gcc -O2 -Wall logs/JoelStageA/joel_laptop/peakws.c -o logs/JoelStageA/joel_laptop/peakws.exe -lpsapi
 * Run:    logs/JoelStageA/joel_laptop/peakws.exe bin/v0_stageA_naive_sma.exe test20 > NUL
 *         -> prints "peakws_kB <number>" on stderr (kB = 1024 bytes, like ru_maxrss on the Pi)
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>          /* GetProcessMemoryInfo, PROCESS_MEMORY_COUNTERS */

int main(int argc, char *argv[]) {
    char cmd[4096] = "";
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    PROCESS_MEMORY_COUNTERS pmc;
    DWORD code = 1;

    if (argc < 2) {
        fprintf(stderr, "usage: %s program [args]\n", argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; i++) {             /* rebuild the command line */
        if (strlen(cmd) + strlen(argv[i]) + 4 >= sizeof cmd) {
            fprintf(stderr, "command line too long\n");
            return 1;
        }
        strcat(cmd, "\"");
        strcat(cmd, argv[i]);
        strcat(cmd, i + 1 < argc ? "\" " : "\"");
    }

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "CreateProcess failed (%lu)\n", GetLastError());
        return 1;
    }
    /* the process handle stays valid after exit, so the counters can still be read */
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof pmc))
        fprintf(stderr, "peakws_kB %llu\n", (unsigned long long)(pmc.PeakWorkingSetSize / 1024));
    else
        fprintf(stderr, "GetProcessMemoryInfo failed (%lu)\n", GetLastError());
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}
