/*
 * device.c - the test set-up the brief asks for (device, CPU, memory, OS,
 * compiler, CPU governor), saved per machine in
 * benchmark/results/<machine>/device.txt as "Key: value" lines, so the final
 * report can show the laptop and the Pi side by side.
 */
#include "bench.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
/* a text value from the Windows registry (HKEY_LOCAL_MACHINE\key) */
static void registry_text(const char *key, const char *name, char *out, DWORD size) {
    if (RegGetValueA(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, NULL, out, &size) != ERROR_SUCCESS)
        out[0] = '\0';
}
#else
/* the first line of a file that starts with prefix ("" = the first line),
 * with the prefix cut off */
static void file_line(const char *path, const char *prefix, char *out, size_t size) {
    char line[512];
    FILE *f = fopen(path, "r");
    out[0] = '\0';
    if (f == NULL) return;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            snprintf(out, size, "%s", line + strlen(prefix));
            out[strcspn(out, "\r\n")] = '\0';
            break;
        }
    }
    fclose(f);
}
#endif

/* collect the set-up and write device.txt */
void save_device_info(void) {
    char path[PATH_LEN], a[256], b[256], c[256];
    time_t now = time(NULL);
    FILE *f;

    snprintf(path, sizeof path, "%s/%s/device.txt", ROOT, MACHINE);
    if ((f = fopen(path, "w")) == NULL) return;
    strftime(a, sizeof a, "%Y-%m-%d %H:%M", localtime(&now));
    fprintf(f, "Measured on: %s\n", a);

#ifdef _WIN32
    registry_text("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer", a, sizeof a);
    registry_text("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName", b, sizeof b);
    fprintf(f, "Device: %s %s\n", a, b);
    registry_text("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", a, sizeof a);
    fprintf(f, "CPU: %s\n", a + strspn(a, " "));
    {
        SYSTEM_INFO si;
        MEMORYSTATUSEX mem;
        GetSystemInfo(&si);
        fprintf(f, "CPU cores (logical): %lu\n", (unsigned long)si.dwNumberOfProcessors);
        mem.dwLength = sizeof mem;
        if (GlobalMemoryStatusEx(&mem))
            fprintf(f, "Memory (RAM): %.1f GB\n", (double)mem.ullTotalPhys / (1024.0 * 1024 * 1024));
    }
    registry_text("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", a, sizeof a);
    registry_text("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion", b, sizeof b);
    registry_text("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild", c, sizeof c);
    if (atoi(c) >= 22000 && strncmp(a, "Windows 10", 10) == 0) a[9] = '1';   /* the registry still  */
    fprintf(f, "OS: %s %s (build %s)\n", a, b, c);                           /* says 10 on Windows 11 */
    fprintf(f, "CPU governor: (Windows power plan - use 'Best performance', plugged in)\n");
#else
    file_line("/proc/device-tree/model", "", a, sizeof a);                    /* e.g. Raspberry Pi 5 Model B */
    fprintf(f, "Device: %s\n", a[0] ? a : "Linux computer");
    first_line("lscpu | grep 'Model name'", a, sizeof a);
    fprintf(f, "CPU: %s\n", strchr(a, ':') ? strchr(a, ':') + 1 + strspn(strchr(a, ':') + 1, " ") : a);
    first_line("nproc", a, sizeof a);
    fprintf(f, "CPU cores (logical): %s\n", a);
    file_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "", a, sizeof a);   /* kHz */
    if (a[0]) fprintf(f, "CPU max clock: %.2f GHz\n", atof(a) / 1e6);
    file_line("/proc/meminfo", "MemTotal:", a, sizeof a);                       /* kB */
    fprintf(f, "Memory (RAM): %.1f GB\n", atof(a) / (1024.0 * 1024));
    file_line("/etc/os-release", "PRETTY_NAME=", c, sizeof c);           /* "Debian GNU/Linux 12 ..." */
    snprintf(a, sizeof a, "%s", c + (c[0] == '"'));                        /* drop the quotes */
    a[strcspn(a, "\"")] = '\0';
    first_line("uname -r", b, sizeof b);
    fprintf(f, "OS: %s (kernel %s)\n", a, b);
    file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "", a, sizeof a);
    fprintf(f, "CPU governor: %s%s\n", a[0] ? a : "(unknown)",
            strcmp(a, "performance") == 0 ? "" : "  <- the brief asks for 'performance'");
#endif
    first_line("gcc --version", a, sizeof a);
    fprintf(f, "Compiler: %s\n", a);
    snprintf(b, sizeof b, "%s --version", PYTHON);
    first_line(b, a, sizeof a);
    fprintf(f, "Python: %s\n", a);
    fprintf(f, "Pinning: %s\n", PIN[0] ? "taskset -c 2 (one CPU core)" : "none");
    fprintf(f, "Dataset: %s\n", DATA_FILE);
    fclose(f);
}

/* copy one machine's device.txt into a summary, indented */
void machine_info(FILE *out) {
    char path[PATH_LEN], line[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s/device.txt", ROOT, MACHINE);
    if ((f = fopen(path, "r")) == NULL) return;
    while (fgets(line, sizeof line, f) != NULL) fprintf(out, "  %s", line);
    fclose(f);
}
