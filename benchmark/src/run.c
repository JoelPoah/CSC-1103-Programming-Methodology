/*
 * run.c - compile, run and time ONE version, and read what it printed
 *
 * All timing uses now_ns() from helper_function/timing.h.
 */
#include "../../helper_function/timing.h"   /* must come first (see timing.h) */
#include "bench.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <psapi.h>                        /* peak memory of a finished program */
#define MAKE_DIR(p)  _mkdir(p)
#define EXIT_CODE(s) (s)                  /* system() already gives the exit code */
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>                 /* getrusage: peak memory of child programs */
#include <unistd.h>
#define MAKE_DIR(p)  mkdir(p, 0755)
#define EXIT_CODE(s) (WIFEXITED(s) ? WEXITSTATUS(s) : -1)
#endif

/* mkdir -p: make every folder on the path (existing ones are fine) */
void mkdir_p(const char *path) {
    char tmp[PATH_LEN];
    size_t i;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') { tmp[i] = '\0'; MAKE_DIR(tmp); tmp[i] = '/'; }
    }
    MAKE_DIR(tmp);
}

int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

/* cmd.exe wants \ in paths, so swap / on Windows */
void native_path(const char *in, char *out, size_t size) {
    snprintf(out, size, "%s", in);
#ifdef _WIN32
    for (char *p = out; *p; p++) if (*p == '/') *p = '\\';
#endif
}

/* run cmd with its output (stdout + stderr) going to out_file.
 * *ms = time from launch to exit, measured with now_ns(). Returns the exit code. */
int run_cmd(const char *cmd, const char *out_file, double *ms) {
    char full[CMD_LEN + PATH_LEN], out[PATH_LEN];
    uint64_t start, end;
    int status;

    native_path(out_file, out, sizeof out);
    snprintf(full, sizeof full, "%s > \"%s\" 2>&1", cmd, out);
    start = now_ns();
    status = system(full);
    end = now_ns();
    if (ms != NULL) *ms = (double)(end - start) / 1e6;
    return EXIT_CODE(status);
}

/*
 * run_cmd_memory - like run_cmd (no timing), but also measures the program's
 * PEAK MEMORY in MB: the most physical memory it used at any moment.
 *   Windows: start the program directly and ask for its peak working set.
 *   Linux/Pi: run it from a fresh child process, whose children are only this
 *             program, and read their maximum resident set size (getrusage).
 * exe must already be a native path. Returns the program's exit code.
 */
#ifdef _WIN32
int run_cmd_memory(const char *exe, const char *args, const char *out_file, double *peak_mb) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };    /* the output file is inherited */
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    PROCESS_MEMORY_COUNTERS pmc;
    char cmdline[CMD_LEN], out[PATH_LEN];
    DWORD code = 1;
    HANDLE file;

    *peak_mb = 0;
    native_path(out_file, out, sizeof out);
    file = CreateFileA(out, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return -1;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;                       /* stdout + stderr -> out_file */
    si.hStdOutput = file;
    si.hStdError = file;
    snprintf(cmdline, sizeof cmdline, "%s %s", exe, args);
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(file);
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    if (K32GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof pmc))
        *peak_mb = (double)pmc.PeakWorkingSetSize / (1024.0 * 1024);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(file);
    return (int)code;
}
#else
int run_cmd_memory(const char *exe, const char *args, const char *out_file, double *peak_mb) {
    char full[CMD_LEN + PATH_LEN];
    int fd[2], status = -1;
    long kb = 0;
    pid_t pid;

    *peak_mb = 0;
    snprintf(full, sizeof full, "%s%s %s > \"%s\" 2>&1", PIN, exe, args, out_file);
    if (pipe(fd) != 0) return -1;
    pid = fork();
    if (pid < 0) { close(fd[0]); close(fd[1]); return -1; }
    if (pid == 0) {                       /* the fresh child: run the program, report its peak */
        struct rusage ru;
        int s = system(full);
        close(fd[0]);
        getrusage(RUSAGE_CHILDREN, &ru);
        kb = ru.ru_maxrss;                /* kB on Linux */
        if (write(fd[1], &kb, sizeof kb) != (ssize_t)sizeof kb) kb = 0;
        _exit(EXIT_CODE(s));
    }
    close(fd[1]);
    if (read(fd[0], &kb, sizeof kb) != (ssize_t)sizeof kb) kb = 0;
    close(fd[0]);
    waitpid(pid, &status, 0);
    *peak_mb = kb / 1024.0;
    return EXIT_CODE(status);
}
#endif

/* perf stat -x, output: "value,unit,event,..." per line -> the four counters */
static void read_perf(const char *path, Result *r) {
    char line[512];
    FILE *f = fopen(path, "r");
    if (f == NULL) return;
    while (fgets(line, sizeof line, f) != NULL) {
        char *end, *event;
        double v = strtod(line, &end);
        if (end == line || *end != ',') continue;            /* "<not counted>" or a comment */
        if ((event = strchr(end + 1, ',')) == NULL) continue; /* skip the unit field */
        event++;
        if (strstr(event, "cache-misses") == event)          r->cache_misses = v;
        else if (strstr(event, "cache-references") == event) r->cache_refs = v;
        else if (strstr(event, "instructions") == event)     r->instructions = v;
        else if (strstr(event, "cycles") == event)           r->cycles = v;
    }
    fclose(f);
    r->has_perf = r->cycles > 0 && r->instructions > 0;
}

/* first line a command prints, e.g. "gcc --version" */
void first_line(const char *cmd, char *out, size_t size) {
    char tmp[PATH_LEN];
    FILE *f;
    snprintf(tmp, sizeof tmp, "%s/.first_line.txt", RESULTS_DIR);
    snprintf(out, size, "(unknown)");
    if (run_cmd(cmd, tmp, NULL) == 0 && (f = fopen(tmp, "r")) != NULL) {
        if (fgets(out, (int)size, f) != NULL) out[strcspn(out, "\r\n")] = '\0';
        fclose(f);
    }
    remove(tmp);
}

/* what a program printed */
typedef struct {
    int has_load, has_proc, has_signals;
    double load, proc;
    long long trades;
    char vwap[32];
    int buys, sells;
} Output;

/* Read the numbers a program printed. section = NULL reads the whole file;
 * otherwise only the lines after an "=== ...section... ===" header, up to
 * the next header (the Stage B/C programs print test20, dev_1M and stress_full). */
static Output read_output(const char *path, const char *section) {
    Output o;
    char line[1024];
    int inside = (section == NULL);
    FILE *f = fopen(path, "r");

    memset(&o, 0, sizeof o);
    if (f == NULL) return o;
    while (fgets(line, sizeof line, f) != NULL) {
        double d;
        long long n;
        if (section != NULL && strncmp(line, "===", 3) == 0) {
            inside = strstr(line, section) != NULL;
            continue;
        }
        if (!inside) continue;
        if (sscanf(line, "Load time: %lf", &d) == 1)          { o.load = d; o.has_load = 1; }
        else if (sscanf(line, "Process time: %lf", &d) == 1)  { o.proc = d; o.has_proc = 1; }
        else if (sscanf(line, "Trades loaded: %lld", &n) == 1) o.trades = n;
        else if (sscanf(line, "Trades: %lld", &n) == 1)        o.trades = n;
        else if (sscanf(line, "Final VWAP: %31s", o.vwap) == 1) ;
        else if (sscanf(line, "Signals: %d BUY, %d SELL", &o.buys, &o.sells) == 2) o.has_signals = 1;
    }
    fclose(f);
    return o;
}

/* record why a version failed; returns 0 so callers can "return fail(...)" */
static int fail(Result *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
    printf("  ! %s\n", r->error);
    return 0;
}

/* PASS if the test20 Final VWAP and BUY/SELL match test20_expected.txt */
static void check_test20(Result *r, const char *path, const char *section) {
    Output got = read_output(path, section);
    Output want = read_output(TEST20_EXPECTED, NULL);
    if (!got.has_signals || !want.has_signals)
        snprintf(r->test20, sizeof r->test20, "not checked");
    else if (strcmp(got.vwap, want.vwap) == 0 && got.buys == want.buys && got.sells == want.sells)
        snprintf(r->test20, sizeof r->test20, "PASS");
    else
        snprintf(r->test20, sizeof r->test20, "FAIL");
}

/*
 * time_version - compile r, check it, then time it "runs" times.
 * Program output is kept in raw_dir. Returns 1 if every step worked.
 */
int time_version(Result *r, int runs, const char *raw_dir) {
    const Program *p = &PROGRAMS[r->prog];
    const char *section = p->own_timer ? DATA_KEY : NULL;
    char out[PATH_LEN], cmd[CMD_LEN + 64], exe[PATH_LEN];
    Output o;
    double ms;
    int i, code;

    /* STEP 1 - compile (gcc's messages go to <id>_gcc.txt) */
    snprintf(out, sizeof out, "%s/%s_gcc.txt", raw_dir, r->id);
    if (run_cmd(r->build_cmd, out, NULL) != 0) return fail(r, "compile failed, see %s", out);

    /* STEP 2 - one warm-up run, not counted: loads the program and the CSV
     * into the cache, and its output gives the answers (VWAP, signals) */
    native_path(r->exe, exe, sizeof exe);
    snprintf(cmd, sizeof cmd, "%s%s", PIN, r->run_cmd);
    snprintf(out, sizeof out, "%s/%s_output.txt", raw_dir, r->id);
    code = run_cmd(cmd, out, &ms);
    if (code != 0) return fail(r, "exit status %d, see %s", code, out);
    o = read_output(out, section);
    r->trades = o.trades;
    r->buys = o.buys;
    r->sells = o.sells;
    snprintf(r->vwap, sizeof r->vwap, "%s", o.vwap);

    /* STEP 3 - correctness: test20 */
    if (p->test_args != NULL) {
        snprintf(cmd, sizeof cmd, "%s %s", exe, p->test_args);
        snprintf(out, sizeof out, "%s/%s_test20.txt", raw_dir, r->id);
        run_cmd(cmd, out, NULL);
        check_test20(r, out, NULL);
    } else {
        check_test20(r, out, "test20");        /* first section of the warm-up output */
    }
    printf("  test20: %s\n", r->test20);

    /* STEP 4 - the timed runs */
    snprintf(cmd, sizeof cmd, "%s%s", PIN, r->run_cmd);
    snprintf(out, sizeof out, "%s/%s_run.txt", raw_dir, r->id);
    for (i = 0; i < runs; i++) {
        code = run_cmd(cmd, out, &ms);
        if (code != 0) return fail(r, "run %d: exit status %d", i + 1, code);
        r->whole[i] = ms;
        if (p->own_timer) {
            o = read_output(out, section);
            if (!o.has_load || !o.has_proc) return fail(r, "no Load/Process time lines in its output");
            r->load[i] = o.load;
            r->proc[i] = o.proc;
            r->time[i] = o.load + o.proc;
            printf("  run %d: %9.1f ms  (load %.1f + process %.1f)\n", i + 1, r->time[i], o.load, o.proc);
        } else {
            r->time[i] = ms;
            printf("  run %d: %9.1f ms\n", i + 1, ms);
        }
    }
    remove(out);

    /* STEP 5 - one more run, not timed: peak memory, and on the Pi the perf stat
     * hardware counters (for the whole program: Stage B/C also do test20 + dev_1M).
     * Not measured on the warm-up: on Windows the first launch of a freshly
     * compiled .exe reads its whole image in once (310 MB instead of 35 MB for
     * stageB_inmem on dev_1M), so it would overstate the memory. */
    {
        char perf_file[PATH_LEN], prog[CMD_LEN];
        snprintf(perf_file, sizeof perf_file, "%s/%s_perf.txt", raw_dir, r->id);
        if (HAVE_PERF)
            snprintf(prog, sizeof prog, "perf stat -x, -e cycles,instructions,cache-references,cache-misses -o %s %s",
                     perf_file, exe);
        else
            snprintf(prog, sizeof prog, "%s", exe);
        code = run_cmd_memory(prog, p->args, out, &r->peak_mb);
        remove(out);
        if (code != 0) return fail(r, "memory run: exit status %d", code);
        printf("  peak memory: %.1f MB\n", r->peak_mb);
        if (HAVE_PERF) {
            read_perf(perf_file, r);
            if (r->has_perf) printf("  perf: IPC %.2f, cache misses %.0f\n", r->instructions / r->cycles, r->cache_misses);
            else printf("  perf stat gave no counters (see %s)\n", perf_file);
        }
    }

    /* STEP 6 - statistics */
    r->n = runs;
    r->split = p->own_timer;
    r->st = stats_of(r->time, runs, 1.0);
    if (r->split) {
        r->st_load = stats_of(r->load, runs, 1.0);
        r->st_proc = stats_of(r->proc, runs, 1.0);
    }
    r->ok = 1;
    printf("  median %.1f ms\n", r->st.median);
    return 1;
}
